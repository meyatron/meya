#include <SoftwareSerial.h>
#include <Servo.h>

// Pins for SoftwareSerial to ESP-01
SoftwareSerial espSerial(2, 3); // RX, TX (Arduino pins connected to ESP-01 TX, RX)
Servo servo;

// Wi-Fi credentials
const char* WIFI_SSID     = "SKYW_90F6_2G";
const char* WIFI_PASSWORD = "PLDTWIFInbdKQ";

// Firebase credentials
const char* FIREBASE_HOST = "autofreshmist-ddc40-default-rtdb.firebaseio.com";
const char* FIREBASE_AUTH = "rUUBBkvBsDo7lrguJMxHyZ1nMDoG9nMRQTOjinqS";

bool lastState = false;

// Spray settings
const unsigned long SPRAY_DURATION = 60000; // 1 minute in milliseconds
const unsigned long INTERVAL = 600000;      // 10 minutes in milliseconds

// ─── Prototypes ──────────────────────────────────────────────────────────────
void   setupESP();
bool   getFirebaseFlag();
void   setFirebaseFlag(bool value);
String httpGET(String url);
void   httpPUT(String url, String jsonPayload);
void   sendCommand(const char* cmd, unsigned long timeout = 2000);
void   waitForPrompt(unsigned long timeout = 3000);
String readResponse(unsigned long timeout = 5000);

// ─── Setup & Loop ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  espSerial.begin(9600);   // match your ESP-01 AT-firmware baud
  servo.attach(9);         // servo signal on pin 9
  servo.write(0);          // initial position
  delay(2000);
  setupESP();
}

void loop() {
  Serial.println("Spraying mist for 1 minute...");
  unsigned long startTime = millis();

  // Move continuously for 1 minute
  while (millis() - startTime < SPRAY_DURATION) {
    servo.write(0);   // Move to position 0
    delay(500);       // Half a second
    servo.write(180); // Move to position 180
    delay(500);       // Half a second
  }

  servo.write(0); // Stop

  Serial.println("Waiting for 10 minutes...");
  delay(INTERVAL); // Wait for 10 minutes
}

// ─── Core Functions ───────────────────────────────────────────────────────────

void setupESP() {
  Serial.println("Resetting ESP...");
  sendCommand("AT+RST", 5000);
  delay(2000);

  sendCommand("AT+CWMODE=1");  // station mode
  String join = "AT+CWJAP=\"" + String(WIFI_SSID) + "\",\"" + String(WIFI_PASSWORD) + "\"";
  Serial.println("Connecting to WiFi...");
  sendCommand(join.c_str(), 15000);

  Serial.println("WiFi connected");
}

bool getFirebaseFlag() {
  String url = "/mister/activate.json?auth=" + String(FIREBASE_AUTH);
  String resp = httpGET(url);
  Serial.println("Firebase GET response: " + resp);
  return resp.indexOf("true") != -1;
}

void setFirebaseFlag(bool value) {
  String url     = "/mister/activate.json?auth=" + String(FIREBASE_AUTH);
  String payload = value ? "true" : "false";
  httpPUT(url, payload);
}

String httpGET(String url) {
  // open TCP
  String startCmd = "AT+CIPSTART=\"TCP\",\"" + String(FIREBASE_HOST) + "\",80";
  sendCommand(startCmd.c_str(), 5000);

  // send GET
  String req = "GET " + url + " HTTP/1.1\r\nHost: " + FIREBASE_HOST + "\r\nConnection: close\r\n\r\n";
  String sendCmd = "AT+CIPSEND=" + String(req.length());
  sendCommand(sendCmd.c_str());
  delay(100);
  waitForPrompt();
  espSerial.print(req);

  // read full response
  String full = readResponse(5000);
  sendCommand("AT+CIPCLOSE");

  int idx = full.indexOf("\r\n\r\n");
  return (idx >= 0) ? full.substring(idx + 4) : "";
}

void httpPUT(String url, String jsonPayload) {
  // open TCP
  String startCmd = "AT+CIPSTART=\"TCP\",\"" + String(FIREBASE_HOST) + "\",80";
  sendCommand(startCmd.c_str(), 5000);

  // send PUT
  String req = 
    "PUT " + url + " HTTP/1.1\r\n" +
    "Host: " + FIREBASE_HOST + "\r\n" +
    "Content-Type: application/json\r\n" +
    "Content-Length: " + String(jsonPayload.length()) + "\r\n" +
    "Connection: close\r\n\r\n" +
    jsonPayload;
  String sendCmd = "AT+CIPSEND=" + String(req.length());
  sendCommand(sendCmd.c_str());
  delay(100);
  waitForPrompt();
  espSerial.print(req);

  delay(1000);
  sendCommand("AT+CIPCLOSE");
}

// ─── Utility Functions ────────────────────────────────────────────────────────
void sendCommand(const char* cmd, unsigned long timeout) {
  espSerial.println(cmd);
  unsigned long start = millis();
  String resp;
  while (millis() - start < timeout) {
    while (espSerial.available()) {
      resp += char(espSerial.read());
    }
    if (resp.indexOf("OK") >= 0 || resp.indexOf("ERROR") >= 0) break;
  }
  Serial.print("AT response: ");
  Serial.println(resp);
}

void waitForPrompt(unsigned long timeout) {
  unsigned long start = millis();
  while (millis() - start < timeout) {
    if (espSerial.find(">")) {
      Serial.println("Got '>' prompt");
      return;
    }
  }
  Serial.println("Timeout waiting for '>' prompt");
}

String readResponse(unsigned long timeout) {
  String data;
  unsigned long start = millis();
  while (millis() - start < timeout) {
    while (espSerial.available()) {
      data += char(espSerial.read());
    }
  }
  return data;
}
