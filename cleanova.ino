// ============================================================
//  Cleanova – Automatic Solar Panel Cleaning System
//  Microcontroller : Arduino (Uno)
//  Light Sensor    : BH1750 (I2C)
//  Motor Driver    : L298N (dual H-bridge)
//  Limit Switches  : 2× normally-open (active LOW with INPUT_PULLUP)
// ============================================================

#include <BH1750.h>
#include <Wire.h>

// ------------------------------------------------------------
// Calibration – adjust these to match your installation
// ------------------------------------------------------------

// Below this level the system assumes it is night → no cleaning
const float LUX_NIGHT_THRESHOLD = 100.0;

// Below this level (but above night) the panel is considered dusty
// Measure a clean panel at noon, then set ~60-70 % of that reading.
// Example: clean reading = 2000 lx → set threshold to ~1200 lx.
// Default below is a reasonable starting point; tune after deployment.
const float LUX_DUST_THRESHOLD = 600.0;

// Motor PWM speed (0–255). 150 ≈ 59 % duty cycle.
const uint8_t MOTOR_SPEED = 150;

// How often the sensor is polled (milliseconds).
// 60 000 ms = 1 minute.  Reduce to 5 000 ms during testing.
const unsigned long POLL_INTERVAL_MS = 60000;

// ------------------------------------------------------------
// Pin assignments – L298N motor driver
// ------------------------------------------------------------
const uint8_t ENA = 5;   // PWM enable – Motor A
const uint8_t ENB = 3;   // PWM enable – Motor B
const uint8_t IN1 = 8;
const uint8_t IN2 = 7;
const uint8_t IN3 = 4;
const uint8_t IN4 = 2;

// ------------------------------------------------------------
// Pin assignments – limit switches (active LOW, INPUT_PULLUP)
// ------------------------------------------------------------
const uint8_t LIMIT_FRONT = 12;  // triggered at the far (dirty) end
const uint8_t LIMIT_BACK  = 13;  // triggered at the rest / home position

// ------------------------------------------------------------
// BH1750 sensor object
// ------------------------------------------------------------
BH1750 lightMeter;

// ============================================================
// Motor helpers
// ============================================================

void moveForward() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  analogWrite(ENA, MOTOR_SPEED);
  analogWrite(ENB, MOTOR_SPEED);
}

void moveBackward() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  analogWrite(ENA, MOTOR_SPEED);
  analogWrite(ENB, MOTOR_SPEED);
}

void stopMotors() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
}

// ============================================================
// Setup
// ============================================================

void setup() {
  Serial.begin(9600);
  Wire.begin();

  // Initialise BH1750 in continuous high-resolution mode
  if (!lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println(F("[ERROR] BH1750 not found. Check wiring (SDA/SCL)."));
    while (true) { delay(1000); }  // halt – cannot proceed without sensor
  }
  Serial.println(F("[INFO] CleaNova system ready."));

  // Motor driver pins
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);

  // Limit switches – use internal pull-up; switch connects pin to GND
  pinMode(LIMIT_FRONT, INPUT_PULLUP);
  pinMode(LIMIT_BACK,  INPUT_PULLUP);

  stopMotors();
}

// ============================================================
// Main loop
// ============================================================

void loop() {
  float lux = lightMeter.readLightLevel();

  Serial.print(F("[SENSOR] Light: "));
  Serial.print(lux);
  Serial.println(F(" lx"));

  // --- Night check: do nothing outside daylight hours ----------
  if (lux < LUX_NIGHT_THRESHOLD) {
    Serial.println(F("[STATUS] Night detected – system idle."));
    delay(POLL_INTERVAL_MS);
    return;  // skip the rest of the loop
  }

  // --- Dust check: clean only when lux drops below threshold ---
 if (lux < LUX_DUST_THRESHOLD) {
    Serial.println(F("[STATUS] Dust detected – starting cleaning pass."));
    
    moveForward();
    bool reversing = false;

    while (true) {
        // Hit far end → reverse
        if (!reversing && digitalRead(LIMIT_FRONT) == LOW) {
            Serial.println(F("[LIMIT] Far end hit – reversing home."));
            moveBackward();
            reversing = true;
        }

        // Hit far end again on the way back → full pass done, stop
        if (reversing && digitalRead(LIMIT_FRONT) == LOW) {
            Serial.println(F("[LIMIT] Full pass complete – stopping."));
            stopMotors();
            break;
        }
    }
}
