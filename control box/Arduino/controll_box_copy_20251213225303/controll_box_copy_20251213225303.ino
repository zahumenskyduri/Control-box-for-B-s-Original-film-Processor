#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Encoder.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Rotary encoder (KY-040)
Encoder myEnc(2, 3);
const int buttonPin = 4;

// Motor control pins (TC1508A – Motor A)
const int IN1 = 8;
const int IN2 = 9;

// State variables
bool running = false;
bool direction = false;
unsigned long startTime = 0;
unsigned long duration = 0;
unsigned long lastSwitch = 0;

// Button handling
bool lastButtonState = HIGH;
unsigned long pressStart = 0;
bool longPressHandled = false;

const int MIN_MINUTES = 1;
const int MAX_MINUTES = 30;
const int DEFAULT_MINUTES = 1;

const unsigned long SWITCH_INTERVAL_MS = 30000UL;
const unsigned long LONG_PRESS_MS = 3000UL;

static inline void setMotor(bool dir) {
  digitalWrite(IN1, dir ? HIGH : LOW);
  digitalWrite(IN2, dir ? LOW  : HIGH);
}

static inline void stopMotor() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
}

static void showMessage(const char* line1, const char* line2, unsigned long ms) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);   // ✅ dôležité
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.print(line1);

  display.setTextSize(1);
  display.setCursor(0, 40);
  display.print(line2);

  display.display();
  delay(ms);
}

void setup() {
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);

  stopMotor();

  // OLED init
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    // ak by zlyhalo, aspoň nenechať bežať motor
    stopMotor();
    while (true) { delay(100); }
  }
  display.setRotation(2);   // 180° rotation (upside down)
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);   // ✅ chýbalo v poslednej verzii
  showMessage("Init...", "", 1000);

  myEnc.write((long)DEFAULT_MINUTES * 4L);
}

void loop() {
  unsigned long now = millis();

  // -------- Button state machine (short press / long press) --------
  bool currentButtonState = digitalRead(buttonPin);

  // Detect press start
  if (lastButtonState == HIGH && currentButtonState == LOW) {
    pressStart = now;
    longPressHandled = false;
  }

  // While held: long press
  if (currentButtonState == LOW && !longPressHandled) {
    if (now - pressStart >= LONG_PRESS_MS) {
      longPressHandled = true;

      // LONG PRESS:
      // - ak bezi: EMERGENCY STOP + reset + menu
      // - ak nebezi: reset encoder na default
      stopMotor();
      running = false;
      myEnc.write((long)DEFAULT_MINUTES * 4L);

      showMessage("STOP", "Reset na 1 min", 600);
    }
  }

  // Detect release -> short press (only if long press wasn't handled)
  bool shortPressed = false;
  if (lastButtonState == LOW && currentButtonState == HIGH) {
    if (!longPressHandled && (now - pressStart) > 30) {
      shortPressed = true;
    }
  }

  lastButtonState = currentButtonState;

  // -------------------- Main logic --------------------
  if (!running) {
    // --- Encoder: drzat 1..30 min ---
    long raw = myEnc.read();
    long rawSteps = raw / 4;
    int minutes = (int)rawSteps;

    if (minutes < MIN_MINUTES) {
      minutes = MIN_MINUTES;
      myEnc.write((long)minutes * 4L);
    } else if (minutes > MAX_MINUTES) {
      minutes = MAX_MINUTES;
      myEnc.write((long)minutes * 4L);
    }

    // UI
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.setTextSize(2);
    display.print("Cas: ");
    display.print(minutes);
    display.println("m");

    display.setTextSize(1);
    display.setCursor(0, 40);
    display.print("Klik=Start");
    display.setCursor(0, 52);
    display.print("Drz 3s=Reset/STOP");
    display.display();

    if (shortPressed) {
      duration = (unsigned long)minutes * 60000UL;
      startTime = now;
      lastSwitch = now;

      direction = false;
      setMotor(direction);   // start hned

      running = true;
      delay(200);
    }

  } else {
    // Running state: switch direction
    if (now - lastSwitch >= SWITCH_INTERVAL_MS) {
      direction = !direction;
      setMotor(direction);
      lastSwitch = now;
    }

    // Remaining time
    unsigned long elapsed = now - startTime;
    unsigned long remaining = (elapsed >= duration) ? 0UL : (duration - elapsed);

    unsigned int remSec = (unsigned int)(remaining / 1000UL);
    unsigned int mm = remSec / 60U;
    unsigned int ss = remSec % 60U;

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.print("Zost:");

    display.setCursor(0, 20);
    if (mm < 10) display.print('0');
    display.print(mm);
    display.print(':');
    if (ss < 10) display.print('0');
    display.print(ss);

    display.setTextSize(1);
    display.setCursor(0, 50);
    display.print("Smer: ");
    display.print(direction ? "B" : "A");
    display.print("  drz 3s=STOP");
    display.display();

    // End by time
    if (elapsed >= duration) {
      stopMotor();
      running = false;
      myEnc.write((long)DEFAULT_MINUTES * 4L);
      delay(600);
    }
  }
}
