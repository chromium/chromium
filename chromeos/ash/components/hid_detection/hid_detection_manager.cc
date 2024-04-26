// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/hid_detection/hid_detection_manager.h"

#include "ash/constants/ash_features.h"

namespace ash::hid_detection {

HidDetectionManager::HidDetectionStatus::HidDetectionStatus(
    InputMetadata pointer_metadata,
    InputMetadata keyboard_metadata,
    bool touchscreen_detected,
    std::optional<BluetoothHidPairingState> pairing_state)
    : pointer_metadata(pointer_metadata),
      keyboard_metadata(keyboard_metadata),
      touchscreen_detected(touchscreen_detected),
      pairing_state(std::move(pairing_state)) {}

HidDetectionManager::HidDetectionStatus::HidDetectionStatus(
    HidDetectionStatus&& other) {
  pointer_metadata = other.pointer_metadata;
  keyboard_metadata = other.keyboard_metadata;
  touchscreen_detected = other.touchscreen_detected;
  pairing_state = std::move(other.pairing_state);
}

HidDetectionManager::HidDetectionStatus&
HidDetectionManager::HidDetectionStatus::operator=(HidDetectionStatus&& other) {
  pointer_metadata = other.pointer_metadata;
  keyboard_metadata = other.keyboard_metadata;
  touchscreen_detected = other.touchscreen_detected;
  pairing_state = std::move(other.pairing_state);
  return *this;
}

HidDetectionManager::HidDetectionStatus::~HidDetectionStatus() = default;

HidDetectionManager::~HidDetectionManager() {
  DCHECK(!delegate_) << " HID detection must be stopped before "
                     << "HidDetectionManager is destroyed";
}

void HidDetectionManager::StartHidDetection(Delegate* delegate) {
  DCHECK(!delegate_);
  delegate_ = delegate;
  PerformStartHidDetection();
}

void HidDetectionManager::StopHidDetection() {
  DCHECK(delegate_);
  PerformStopHidDetection();
  delegate_ = nullptr;
}

void HidDetectionManager::NotifyHidDetectionStatusChanged() {
  delegate_->OnHidDetectionStatusChanged(ComputeHidDetectionStatus());
}

}  // namespace ash::hid_detection
