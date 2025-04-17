// Define the PWM pins for the single BTS7960 motor driver
const int RPWM = 5;
const int LPWM = 6;

// Current control parameters
const float supplyVoltage = 12.0;
const float coilResistance = 1.0;
float peakCurrent = 5.0;
int motorPower = 0;

// BTS7960 driver compensation factor
const float driverCompensationFactor = 2.5;  // Adjust based on testing

// Frequency and timing parameters

const float frequencyHz = 0.02;
float angle = 0.0;
int delayTime;
float periodMs;
int peakHoldTime = 1000;// youyi this is time at peak in ms


// Wave control parameters
int totalWaves = 3;
int currentWave = 0;
boolean generatingWaves = false;

// Operation mode
enum OperationMode { UP_ONLY, UP_DOWN };
OperationMode opMode = UP_DOWN;  // Default to bidirectional

// Serial command buffer
String cmdBuffer = "";
boolean cmdComplete = false;

void setup() {
  pinMode(RPWM, OUTPUT);
  pinMode(LPWM, OUTPUT);
  
  Serial.begin(9600);
  
  // Calculate PWM value based on desired current
  motorPower = calculatePwmForCurrent(peakCurrent);
  
  periodMs = 1000.0 / frequencyHz;
  delayTime = periodMs / 150.0;
  
  // Display initial settings
  displayCurrentSettings();
  
  Serial.println("Commands: ");
  Serial.println("C<value> - Set peak current in amps (e.g., C5)");
  Serial.println("W<value> - Set number of waves to generate (e.g., W8)");
  Serial.println("UP - Set mode to clockwise only (absolute sine wave)");
  Serial.println("UPDOWN - Set mode to bidirectional (normal sine wave)");
  Serial.println("S - Start generating waves");
  Serial.println("H - Halt/stop wave generation");
  Serial.println("D - Display current settings");
  Serial.println("T<value> - Set peak hold time in ms (e.g., T1000)");
}

void displayCurrentSettings() {
  Serial.println("\n------ CURRENT SETTINGS ------");
  Serial.print("Peak Current: ");
  Serial.print(peakCurrent);
  Serial.println(" A");
  Serial.print("Number of Waves: ");
  Serial.println(totalWaves);
  Serial.print("Frequency: ");
  Serial.print(frequencyHz);
  Serial.println(" Hz");
  Serial.print("PWM Value: ");
  Serial.print(motorPower);
  Serial.println(" (0-255)");
  Serial.print("Mode: ");
  Serial.println(opMode == UP_ONLY ? "UP (clockwise only)" : "UPDOWN (bidirectional)");
  Serial.print("Peak Hold Time: ");
  Serial.print(peakHoldTime);
  Serial.println(" ms");
  Serial.println("----------------------------\n");
}

int calculatePwmForCurrent(float targetCurrent) {
  // Limit current to what the power supply can provide
  float maxCurrent = supplyVoltage / coilResistance;
  if (targetCurrent > maxCurrent) {
    targetCurrent = maxCurrent;
    Serial.print("Warning: Current limited to maximum: ");
    Serial.print(maxCurrent);
    Serial.println("A");
  }
  
  // Calculate the needed voltage with compensation factor
  float voltageNeeded = targetCurrent * coilResistance * driverCompensationFactor;
  
  // Convert to PWM value (0-255)
  int pwmValue = (voltageNeeded / supplyVoltage) * 255;
  
  // Ensure the value is in valid range
  return constrain(pwmValue, 0, 255);
}

void controlMotor(int power) {
  power = constrain(power, -255, 255);
  
  if (power > 0) {
    analogWrite(RPWM, power);
    analogWrite(LPWM, 0);
  } else if (power < 0) {
    analogWrite(RPWM, 0);
    analogWrite(LPWM, -power);
  } else {
    analogWrite(RPWM, 0);
    analogWrite(LPWM, 0);
  }
}

bool sinusoidalPeakControl() {
  float sineValue = sin(angle);
  float motorSinePower;
  
  // Apply the selected operation mode
  if (opMode == UP_ONLY) {
    // Absolute value of sine for UP_ONLY mode (clockwise only)
    motorSinePower = motorPower * abs(sineValue);
  } else {
    // Normal sine wave for UP_DOWN mode (bidirectional)
    motorSinePower = motorPower * sineValue;
  }
  
  // Control the motor
  controlMotor((int)motorSinePower);
  
  // Peak hold logic
  if (abs(motorSinePower) > motorPower * 0.98) {
    delay(peakHoldTime);
  } else {
    delay(delayTime);
  }
  
  // Increment angle
  angle += (2 * PI * delayTime) / periodMs;
  
  // Check if we've completed a wave
  if (angle >= 2 * PI) {
    angle -= 2 * PI;
    currentWave++;
    
    Serial.print("Wave ");
    Serial.print(currentWave);
    Serial.print(" of ");
    Serial.print(totalWaves);
    Serial.println(" completed");
    
    if (currentWave >= totalWaves && totalWaves > 0) {
      controlMotor(0);
      currentWave = 0;
      generatingWaves = false;
      Serial.println("Completed wave generation");
      return false;
    }
  }
  return true;
}

// Function to read characters from Serial and build a command
void serialEvent() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    
    // Add character to buffer if it's not a newline or carriage return
    if (inChar != '\n' && inChar != '\r') {
      cmdBuffer += inChar;
    }
    
    // Mark command as complete when newline received
    if (inChar == '\n') {
      cmdComplete = true;
    }
  }
}

void processCommand() {
  if (cmdComplete) {
    cmdBuffer.trim(); // Remove any whitespace
    
    // Check for mode commands first (these are longer than single letters)
    if (cmdBuffer.equalsIgnoreCase("UP")) {
      opMode = UP_ONLY;
      Serial.println("Mode set to UP (clockwise only)");
      displayCurrentSettings();
    } 
    else if (cmdBuffer.equalsIgnoreCase("UPDOWN")) {
      opMode = UP_DOWN;
      Serial.println("Mode set to UPDOWN (bidirectional)");
      displayCurrentSettings();
    }
    // Now check for single-letter commands
    else if (cmdBuffer.length() > 0) {
      char command = cmdBuffer.charAt(0);
      String value = "";
      if (cmdBuffer.length() > 1) {
        value = cmdBuffer.substring(1);
      }
      
      switch (command) {
        case 'C':
        case 'c':
          if (value.length() > 0) {
            float newCurrent = value.toFloat();
            if (newCurrent > 0) {
              peakCurrent = newCurrent;
              motorPower = calculatePwmForCurrent(peakCurrent);
              Serial.print("New current set to: ");
              Serial.print(peakCurrent);
              Serial.print("A, PWM value: ");
              Serial.println(motorPower);
              displayCurrentSettings();
            }
          }
          break;
          
        case 'W':
        case 'w':
          if (value.length() > 0) {
            int newWaveCount = value.toInt();
            if (newWaveCount >= 0) {
              totalWaves = newWaveCount;
              Serial.print("Wave count set to: ");
              Serial.println(totalWaves);
              displayCurrentSettings();
            }
          }
          break;
        case 'T':
        case 't':
          if (value.length() > 0) {
            peakHoldTime = value.toInt();
            Serial.print("Peak hold time set to: ");
            Serial.print(peakHoldTime);
            Serial.println(" ms");
            displayCurrentSettings();
          }
          break;
        case 'S':
        case 's':
          currentWave = 0;
          angle = 0;
          generatingWaves = true;
          Serial.print("Starting to generate ");
          Serial.print(totalWaves);
          Serial.println(" waves");
          break;
          
        case 'H':
        case 'h':
          generatingWaves = false;
          controlMotor(0);
          Serial.println("Wave generation halted");
          break;
        
        case 'D':
        case 'd':
          displayCurrentSettings();
          break;
          
        default:
          Serial.println("Unknown command. Available commands:");
          Serial.println("C<value> - Set peak current in amps (e.g., C5)");
          Serial.println("W<value> - Set number of waves to generate (e.g., W8)");
          Serial.println("UP - Set mode to clockwise only (absolute sine wave)");
          Serial.println("UPDOWN - Set mode to bidirectional (normal sine wave)");
          Serial.println("S - Start generating waves");
          Serial.println("H - Halt/stop wave generation");
          Serial.println("D - Display current settings");
          Serial.println("T<value> - Set peak hold time in ms (e.g., T1000)");
          break;
      }
    }
    
    // Reset for next command
    cmdBuffer = "";
    cmdComplete = false;
  }
}

void loop() {
  // Check and process any serial commands
  serialEvent();  // Read any available serial data
  processCommand();  // Process any complete command
  
  // Generate waves if the flag is set
  if (generatingWaves) {
    generatingWaves = sinusoidalPeakControl();
  }
}