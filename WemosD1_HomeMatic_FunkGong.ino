#include <Arduino.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <FastLED.h>

FASTLED_USING_NAMESPACE

#if defined(FASTLED_VERSION) && (FASTLED_VERSION < 3001000)
#warning "Requires FastLED 3.1 or later"
#endif

#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
#define NUM_LEDS    16

#define RelaisPin D6
#define BusyPin   D5
#define DataPin   D7
#define KeyPin    D8

SoftwareSerial DFSerial(D2, D1);
DFRobotDFPlayerMini myDFPlayer;
const char* ssid = "";
const char* key = "";
WiFiUDP UDP;
unsigned long PlayStartMillis = 0;
CRGB LEDs[NUM_LEDS];
unsigned long LEDRunningMillis = 0;

struct values_s {
  int PlayNum = 0;
  int PlayVol = 0;
  String LEDColor = "000000";
  int LEDBrightness = 0;
  int FadeBPM = 0;
  unsigned int LEDTimeout = 0;
} Values;

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(RelaisPin, OUTPUT);
  pinMode(BusyPin, INPUT_PULLUP);
  pinMode(KeyPin, INPUT_PULLUP);
  digitalWrite(RelaisPin, HIGH);

  DFSerial.begin(9600);
  Serial.begin(115200);
  doWifiConnect();
  UDP.begin(6899);

  Serial.println();
  Serial.println("Initializing DFPlayer ...");

  if (!myDFPlayer.begin(DFSerial)) {
    Serial.println("DFPlayer Init Error.");
    ESP.restart();
  }
  Serial.println("DFPlayer Mini online.");
  myDFPlayer.volume(0);

  FastLED.addLeds<LED_TYPE, DataPin, COLOR_ORDER>(LEDs, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(Values.LEDBrightness);
  fill_solid(LEDs, NUM_LEDS, CRGB::Black);
  FastLED.show();
}


void loop() {
  if (LEDRunningMillis > 0) {
    EVERY_N_MILLISECONDS(20) {
      FadeLED();
    }
  }

  FastLED.show();
  if (WiFi.status() == WL_CONNECTED) {
    if (PlayStartMillis != 0 && (millis() - PlayStartMillis > 2500) && digitalRead(BusyPin) == HIGH) {
      digitalWrite(RelaisPin, HIGH);
      PlayStartMillis = 0;
    }
    parseUDP();
    if (Values.PlayNum > 0) {
      Serial.println("DFPlayer Playing #" + String(Values.PlayNum) + " Volume = " + String(Values.PlayVol));
      LEDRunningMillis = millis();
      myDFPlayer.volume(Values.PlayVol);
      digitalWrite(RelaisPin, LOW);
      delay(100);
      myDFPlayer.playMp3Folder(Values.PlayNum);
      Values.PlayNum = 0;
      PlayStartMillis = millis();
    }
  } else {
    ESP.restart();
  }

  if ((digitalRead(KeyPin) == HIGH) || (millis() - LEDRunningMillis > Values.LEDTimeout && LEDRunningMillis > 0)) {
    LEDRunningMillis = 0;
    fill_solid(LEDs, NUM_LEDS, CRGB::Black);
  }
}

bool doWifiConnect() {
  Serial.println("Connecting WLAN...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, key);
  int waitCounter = 0;
  digitalWrite(LED_BUILTIN, LOW);
  //statische Adresse: WiFi.config(IPAddress(192,168,1,200), IPAddress(192,168,1,1), IPAddress(255,255,255,0), IPAddress(192,168,1,1));
  while (WiFi.status() != WL_CONNECTED) {
    waitCounter++;
    Serial.println("Wifi not connected");
    if (waitCounter == 20) {
      digitalWrite(LED_BUILTIN, HIGH);
      ESP.restart();
    }
    delay(500);
  }
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println("Wifi Connected");
  Serial.print(WiFi.localIP());
  return true;
}

void parseUDP() {
  char incomingPacket[255];
  int packetSize = UDP.parsePacket();
  if (packetSize) {
    int len = UDP.read(incomingPacket, 255);
    if (len > 0)
      incomingPacket[len] = 0;

    char delimiter[] = ",;";
    char *ptr;

    ptr = strtok(incomingPacket, delimiter);
    Values.PlayNum = atoi(ptr);
    byte paramCount = 0;
    while (ptr != NULL) {
      paramCount++;
      ptr = strtok(NULL, delimiter );
      if (paramCount == 1) Values.PlayVol = atoi(ptr);
      if (paramCount == 2) Values.LEDColor = String(ptr);
      if (paramCount == 3) Values.LEDBrightness = atoi(ptr);
      if (paramCount == 4) Values.FadeBPM = atoi(ptr);
      if (paramCount == 5) Values.LEDTimeout = atoi(ptr) * 1000;
    }

    Serial.println("Got new Values from UDP:");
    Serial.println("Values.PlayNum = " + String(Values.PlayNum));
    Serial.println("Values.PlayVol = " + String(Values.PlayVol));
    Serial.println("Values.LEDColor = " + Values.LEDColor);
    Serial.println("Values.LEDBrightness = " + String(Values.LEDBrightness));
    Serial.println("Values.FadeBPM = " + String(Values.FadeBPM));
    Serial.println("Values.LEDTimeout = " + String(Values.LEDTimeout));
    FastLED.setBrightness(Values.LEDBrightness);
  }
}

void FadeLED() {
  fadeToBlackBy(LEDs, NUM_LEDS, 25);
  byte pos = (beat8(Values.FadeBPM) * NUM_LEDS) / 255;
  char hexArray[24];
  Values.LEDColor.toCharArray(hexArray, 24);
  LEDs[pos] = x2i(hexArray);
}

int x2i(char *s) {
  int x = 0;
  for (;;) {
    char c = *s;
    if (c >= '0' && c <= '9') {
      x *= 16;
      x += c - '0';
    }
    else if (c >= 'A' && c <= 'F') {
      x *= 16;
      x += (c - 'A') + 10;
    }
    else break;
    s++;
  }
  return x;
}

