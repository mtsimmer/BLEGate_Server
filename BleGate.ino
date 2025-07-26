#include <SSD1306Wire.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// OLED Configuration
#define OLED_SDA 5
#define OLED_SCL 4
SSD1306Wire display(0x3c, OLED_SDA, OLED_SCL);
bool displayActive = true;

// GPIO Configuration
const int BUTTON_PIN = 16;
unsigned long lastPressTime = 0;

// BLE Configuration
const String SECRET = "Fuck0ferGates";
const char* DEVICE_NAME = "SecureGateController";
bool deviceConnected = false;
bool authenticated = false;
String writtenValue = "";
int openCount = 0;
String currentState = "Closed";

// ⬇️ Updated UUIDs
#define SERVICE_UUID               "7d63e895-9dab-46cc-b55d-2a1a71469d3a"
#define CHARACTERISTIC_NOTIFY_UUID "575acadf-8ee3-43be-be4c-fba91b324e45"
#define CHARACTERISTIC_WRITE_UUID  "69aeed94-cf0d-4822-9e50-7de8e84e2d91"

// Timing Configuration
const unsigned long PRESS_DURATION = 2000;
const unsigned long PRESS_VARIATION = 200;
const unsigned long GATE_CLOSE_TIMEOUT = 60000;
const unsigned long SCREEN_OFF_TIMEOUT = 60000;

// Function declarations
void triggerButtonPress();
void updateDisplay();
void checkStateTransitions();

class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    displayActive = true;
    currentState = "Connected";
  }

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    authenticated = false;
    displayActive = true;
    currentState = "Disconnected";
    BLEDevice::startAdvertising();
  }
};

class WriteCallback: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    writtenValue = String(value.c_str()).substring(0, 32);
    displayActive = true;

    if (writtenValue == SECRET) {
      authenticated = true;
      writtenValue = "";
      pCharacteristic->setValue(""); // Clear after validating
      triggerButtonPress();
    } else {
      writtenValue = "Wrong Secret!";
    }
  }
};

BLECharacteristic* notifyChar;

void triggerButtonPress() {
  unsigned long pressTime = PRESS_DURATION + random(-PRESS_VARIATION, PRESS_VARIATION);
  digitalWrite(BUTTON_PIN, LOW);
  currentState = "Opening...";
  delay(pressTime);
  digitalWrite(BUTTON_PIN, HIGH);

  openCount++;
  currentState = "Opened (" + String(openCount) + ")";
  if (notifyChar) {
    notifyChar->setValue(currentState.c_str());
    notifyChar->notify();
  }

  lastPressTime = millis();
}

void updateDisplay() {
  display.clear();
  display.setFont(ArialMT_Plain_16);

  display.drawString(0, 0, deviceConnected ? "Connected" : "Disconnected");
  display.drawString(0, 20, currentState);
  display.drawString(0, 40, writtenValue.length() ? writtenValue : "Ready");

  display.display();
}

void checkStateTransitions() {
  unsigned long now = millis();

  if (now - lastPressTime >= GATE_CLOSE_TIMEOUT && openCount > 0) {
    openCount = 0;
    currentState = "Closed";
    if (notifyChar) {
      notifyChar->setValue("Closed");
      notifyChar->notify();
    }
  }

  if (now - lastPressTime >= SCREEN_OFF_TIMEOUT && displayActive) {
    display.displayOff();
    displayActive = false;
  }
}

void setup() {
  Serial.begin(115200);

  // GPIO
  pinMode(BUTTON_PIN, OUTPUT);
  digitalWrite(BUTTON_PIN, HIGH);

  // OLED
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 0, "Initializing...");
  display.display();

  // BLE
  BLEDevice::init(DEVICE_NAME);
  BLEServer* pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService* pService = pServer->createService(SERVICE_UUID);

  notifyChar = pService->createCharacteristic(
    CHARACTERISTIC_NOTIFY_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  notifyChar->addDescriptor(new BLE2902());

  BLECharacteristic* writeChar = pService->createCharacteristic(
    CHARACTERISTIC_WRITE_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  writeChar->setCallbacks(new WriteCallback());

  pService->start();

  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  BLEDevice::startAdvertising();
}

void loop() {
  if (displayActive) {
    updateDisplay();
  }

  checkStateTransitions();

  if (deviceConnected && authenticated) {
    authenticated = false; // Reset after processing
  }

  delay(100);
}
