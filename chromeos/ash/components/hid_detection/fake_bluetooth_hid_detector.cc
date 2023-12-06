// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/hid_detection/fake_bluetooth_hid_detector.h"

namespace ash::hid_detection {

FakeBluetoothHidDetector::FakeBluetoothHidDetector() = default;

FakeBluetoothHidDetector::~FakeBluetoothHidDetector() = default;

void FakeBluetoothHidDetector::SetInputDevicesStatus(
    InputDevicesStatus input_devices_status) {
  input_devices_status_ = input_devices_status;
  num_set_input_devices_status_calls_++;
}

const BluetoothHidDetector::BluetoothHidDetectionStatus
FakeBluetoothHidDetector::GetBluetoothHidDetectionStatus() {
  std::optional<BluetoothHidMetadata> current_pairing_device;
  if (current_pairing_device_.has_value()) {
    current_pairing_device =
        BluetoothHidMetadata{current_pairing_device_.value().name,
                             current_pairing_device_.value().type};
  }

  std::optional<BluetoothHidPairingState> pairing_state;
  if (current_pairing_state_.has_value()) {
    pairing_state = BluetoothHidPairingState{
        current_pairing_state_.value().code,
        current_pairing_state_.value().num_keys_entered};
  }

  return BluetoothHidDetectionStatus{std::move(current_pairing_device),
                                     std::move(pairing_state)};
}

void FakeBluetoothHidDetector::SimulatePairingStarted(
    BluetoothHidDetector::BluetoothHidMetadata pairing_device) {
  current_pairing_device_ = std::move(pairing_device);
  NotifyBluetoothHidDetectionStatusChanged();
}

void FakeBluetoothHidDetector::SetPairingState(
    std::optional<BluetoothHidPairingState> pairing_state) {
  current_pairing_state_ = std::move(pairing_state);
  NotifyBluetoothHidDetectionStatusChanged();
}

void FakeBluetoothHidDetector::SimulatePairingSessionEnded() {
  current_pairing_device_.reset();
  current_pairing_state_.reset();
  NotifyBluetoothHidDetectionStatusChanged();
}

void FakeBluetoothHidDetector::PerformStartBluetoothHidDetection(
    InputDevicesStatus input_devices_status) {
  input_devices_status_ = input_devices_status;
  is_bluetooth_hid_detection_active_ = true;
}

void FakeBluetoothHidDetector::PerformStopBluetoothHidDetection(
    bool is_using_bluetooth) {
  is_bluetooth_hid_detection_active_ = false;
  is_using_bluetooth_ = is_using_bluetooth;
}

}  // namespace ash::hid_detection
