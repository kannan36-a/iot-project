#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include "ThingSpeak.h"

// ── OLED ──────────────────────────────
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ── Temperature (no resistor) ─────────
#define TEMP_PIN 4
OneWire oneWire(TEMP_PIN);
DallasTemperature sensors(&oneWire);

// ── pH ────────────────────────────────
#define PH_PIN 34

// ── Wi-Fi ─────────────────────────────
const char* ssid     = "ROBER";
const char* password = "YOUthink";

// ── ThingSpeak ────────────────────────
unsigned long channelID = 3279134;
const char* apiKey      = "AKMGGA7LNHK2RWFI";
WiFiClient client;

// ── Timing ────────────────────────────
unsigned long lastUpload = 0;
const long uploadInterval = 15000;

// ─────────────────────────────────────
void setup() {
  Serial.begin(115200);
  analogReadResolution(12);

  // internal pull-up for DS18B20 (no resistor needed)
  pinMode(TEMP_PIN, INPUT_PULLUP);

  // Start OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found!");
    while (true);
  }
  display.clearDisplay();
  display.setTextColor(WHITE);

  // Start temperature sensor
  sensors.begin();

  // Connect Wi-Fi
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Connecting WiFi...");
  display.display();

  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WiFi Connected!");
    display.display();
    delay(1500);
  } else {
    Serial.println("\nWiFi Failed!");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WiFi Failed!");
    display.println("Check credentials");
    display.display();
    delay(2000);
  }

  // Start ThingSpeak
  ThingSpeak.begin(client);
}

// ─────────────────────────────────────
void loop() {

  // ── Read Temperature ────────────────
  sensors.requestTemperatures();
  float temp = sensors.getTempCByIndex(0);
  if (temp == -127.0) {
    Serial.println("Temp sensor error!");
    temp = 0.0;
  }

  // ── Read pH ─────────────────────────
  int rawPH = 0;
  for (int i = 0; i < 10; i++) {
    rawPH += analogRead(PH_PIN);
    delay(10);
  }
  rawPH /= 10;
  float voltage = (rawPH / 4095.0) * 3.3;
  float pH = (-5.70 * voltage) + 21.34; // ✅ corrected formula

  // clamp between 0 and 14
  if (pH < 0)  pH = 0;
  if (pH > 14) pH = 14;

  // ── Show on OLED ────────────────────
  display.clearDisplay();

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("-- Water Monitor --");

  display.setTextSize(2);
  display.setCursor(0, 16);
  display.print("pH:");
  display.println(pH, 1);

  display.setCursor(0, 38);
  display.print("T:");
  display.print(temp, 1);
  display.println("C");

  display.setTextSize(1);
  display.setCursor(0, 56);
  if (pH >= 6.5 && pH <= 8.5) {
    display.println("Status: NORMAL");
  } else {
    display.println("Status: CHECK pH!");
  }

  display.display();

  // ── Serial Monitor ──────────────────
  Serial.print("pH: ");     Serial.print(pH, 2);
  Serial.print("  Temp: "); Serial.print(temp, 1);
  Serial.println(" C");

  // ── Upload to ThingSpeak ─────────────
  if (millis() - lastUpload >= uploadInterval) {
    if (WiFi.status() == WL_CONNECTED) {
      ThingSpeak.setField(1, pH);
      ThingSpeak.setField(2, temp);

      int result = ThingSpeak.writeFields(channelID, apiKey);

      if (result == 200) {
        Serial.println("ThingSpeak: Upload OK!");
      } else {
        Serial.println("ThingSpeak Error: " + String(result));
      }
    } else {
      Serial.println("WiFi disconnected!");
    }
    lastUpload = millis();
  }

  delay(2000);
}