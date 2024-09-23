// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/hid_detection/bluetooth_hid_detector.h"

#include "ash/constants/ash_features.h"

namespace ash::hid_detection {

BluetoothHidPairingState::BluetoothHidPairingState(const std::string& code,
                                                   uint8_t num_keys_entered)
    : code(code), num_keys_entered(num_keys_entered) {}

BluetoothHidPairingState::BluetoothHidPairingState(
    BluetoothHidPairingState&& other) {
  code = std::move(other.code);
  num_keys_entered = other.num_keys_entered;
}

BluetoothHidPairingState& BluetoothHidPairingState::operator=(
    BluetoothHidPairingState&& other) {
  code = std::move(other.code);
  num_keys_entered = other.num_keys_entered;
  return *this;
}

BluetoothHidPairingState::~BluetoothHidPairingState() = default;

BluetoothHidDetector::BluetoothHidMetadata::BluetoothHidMetadata(
    std::string name,
    BluetoothHidType type)
    : name(std::move(name)), type(type) {}

BluetoothHidDetector::BluetoothHidMetadata::BluetoothHidMetadata(
    BluetoothHidMetadata&& other) {
  name = std::move(other.name);
  type = other.type;
}

BluetoothHidDetector::BluetoothHidMetadata&
BluetoothHidDetector::BluetoothHidMetadata::operator=(
    BluetoothHidMetadata&& other) {
  name = std::move(other.name);
  type = other.type;
  return *this;
}

BluetoothHidDetector::BluetoothHidMetadata::~BluetoothHidMetadata() = default;

BluetoothHidDetector::BluetoothHidDetectionStatus::BluetoothHidDetectionStatus(
    std::optional<BluetoothHidDetector::BluetoothHidMetadata>
        current_pairing_device,
    std::optional<BluetoothHidPairingState> pairing_state)
    : current_pairing_device(std::move(current_pairing_device)),
      pairing_state(std::move(pairing_state)) {}

BluetoothHidDetector::BluetoothHidDetectionStatus::BluetoothHidDetectionStatus(
    BluetoothHidDetectionStatus&& other) {
  current_pairing_device = std::move(other.current_pairing_device);
  pairing_state = std::move(other.pairing_state);
}

BluetoothHidDetector::BluetoothHidDetectionStatus&
BluetoothHidDetector::BluetoothHidDetectionStatus::operator=(
    BluetoothHidDetectionStatus&& other) {
  current_pairing_device = std::move(other.current_pairing_device);
  pairing_state = std::move(other.pairing_state);
  return *this;
}

BluetoothHidDetector::BluetoothHidDetectionStatus::
    ~BluetoothHidDetectionStatus() = default;

BluetoothHidDetector::~BluetoothHidDetector() {
  DCHECK(!delegate_) << " Bluetooth HID detection must be stopped before "
                     << "BluetoothHidDetector is destroyed.";
}

void BluetoothHidDetector::StartBluetoothHidDetection(
    Delegate* delegate,
    InputDevicesStatus input_devices_status) {
  DCHECK(!delegate_);
  delegate_ = delegate;
  PerformStartBluetoothHidDetection(input_devices_status);
}

void BluetoothHidDetector::StopBluetoothHidDetection(bool is_using_bluetooth) {
  DCHECK(delegate_);
  PerformStopBluetoothHidDetection(is_using_bluetooth);
  delegate_ = nullptr;
}

void BluetoothHidDetector::NotifyBluetoothHidDetectionStatusChanged() {
  delegate_->OnBluetoothHidStatusChanged();
}

}  // namespace ash::hid_detection
