// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_HID_DETECTION_FAKE_BLUETOOTH_HID_DETECTOR_H_
#define CHROMEOS_ASH_COMPONENTS_HID_DETECTION_FAKE_BLUETOOTH_HID_DETECTOR_H_

#include <optional>

#include "chromeos/ash/components/hid_detection/bluetooth_hid_detector.h"

namespace ash::hid_detection {

class FakeBluetoothHidDetector : public BluetoothHidDetector {
 public:
  FakeBluetoothHidDetector();
  ~FakeBluetoothHidDetector() override;

  // BluetoothHidDetector:
  void SetInputDevicesStatus(InputDevicesStatus input_devices_status) override;
  const BluetoothHidDetectionStatus GetBluetoothHidDetectionStatus() override;

  void SimulatePairingStarted(
      BluetoothHidDetector::BluetoothHidMetadata pairing_device);
  void SetPairingState(std::optional<BluetoothHidPairingState> pairing_state);
  void SimulatePairingSessionEnded();

  const InputDevicesStatus& input_devices_status() {
    return input_devices_status_;
  }

  size_t num_set_input_devices_status_calls() {
    return num_set_input_devices_status_calls_;
  }

  bool is_bluetooth_hid_detection_active() {
    return is_bluetooth_hid_detection_active_;
  }

  bool is_using_bluetooth() { return is_using_bluetooth_; }

 private:
  // BluetoothHidDetector:
  void PerformStartBluetoothHidDetection(
      InputDevicesStatus input_devices_status) override;
  void PerformStopBluetoothHidDetection(bool is_using_bluetooth) override;

  InputDevicesStatus input_devices_status_;
  size_t num_set_input_devices_status_calls_ = 0;

  std::optional<BluetoothHidMetadata> current_pairing_device_;
  std::optional<BluetoothHidPairingState> current_pairing_state_;
  bool is_bluetooth_hid_detection_active_ = false;
  bool is_using_bluetooth_ = false;
};

}  // namespace ash::hid_detection

#endif  // CHROMEOS_ASH_COMPONENTS_HID_DETECTION_FAKE_BLUETOOTH_HID_DETECTOR_H_
