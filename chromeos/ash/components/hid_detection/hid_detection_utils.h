// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_HID_DETECTION_HID_DETECTION_UTILS_H_
#define CHROMEOS_ASH_COMPONENTS_HID_DETECTION_HID_DETECTION_UTILS_H_

#include "services/device/public/mojom/input_service.mojom.h"

namespace ash::hid_detection {

// This enum is tied directly to the HidType UMA enum defined in
// //tools/metrics/histograms/enums.xml, and should always reflect it (do not
// change one without changing the other).
enum class HidType {
  kTouchscreen = 0,
  kUsbKeyboard = 1,
  kUsbPointer = 2,
  kSerialKeyboard = 3,
  kSerialPointer = 4,
  kBluetoothKeyboard = 5,
  kBluetoothPointer = 6,
  kUnknownKeyboard = 7,
  kUnknownPointer = 8,
  kMaxValue = kUnknownPointer
};

// This enum is tied directly to the HidsMissing UMA enum defined in
// //tools/metrics/histograms/enums.xml, and should always reflect it (do not
// change one without changing the other).
enum class HidsMissing {
  kNone = 0,
  kPointer = 1,
  kKeyboard = 2,
  kPointerAndKeyboard = 3,
  kMaxValue = kPointerAndKeyboard
};

// This enum is tied directly to the HidDetectionBluetoothPairingResult UMA
// enum defined in //tools/metrics/histograms/enums.xml, and should always
// reflect it (do not add cases to one without adding cases to the other).
enum class HidDetectionBluetoothPairingResult {
  kNotPaired = 0,  // Attempted but did not pair via Bluetooth
  kPaired = 1,     // Paired via Bluetooth
  kMaxValue = kPaired
};

// Returns true if |device| is a HID with pointing capabilities (i.e. a mouse or
// touchpad).
bool IsDevicePointer(const device::mojom::InputDeviceInfo& device);

// Returns true if |device| is a HID with touchscreen capabilities (i.e. a
// touchscreen or tablet).
bool IsDeviceTouchscreen(const device::mojom::InputDeviceInfo& device);

// Record each HID that is connected while the HID detection screen is shown.
void RecordHidConnected(const device::mojom::InputDeviceInfo& device);

// Record each HID that is disconnected while the HID detection screen is shown.
void RecordHidDisconnected(const device::mojom::InputDeviceInfo& device);

// Record the total number of bluetooth pairing attempts while the HID detection
// is shown.
void RecordBluetoothPairingAttempts(size_t attempts);

// Record the amount of time taken to pair with a Bluetooth device and the
// result of the pairing process during the HID detection automatic pairing
// process.
void RecordBluetoothPairingResult(bool success,
                                  base::TimeDelta pairing_duration);

// Record each HID that is missing when the HID detection screen is shown.
void RecordInitialHidsMissing(const HidsMissing& hids_missing);

// Record each time a pairing session exceeds the timeout.
void RecordPairingTimeoutExceeded();

}  // namespace ash::hid_detection

#endif  // CHROMEOS_ASH_COMPONENTS_HID_DETECTION_HID_DETECTION_UTILS_H_
