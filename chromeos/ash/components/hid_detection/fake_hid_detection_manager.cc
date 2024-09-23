// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/hid_detection/fake_hid_detection_manager.h"

namespace ash::hid_detection {
namespace {

bool IsInputMissing(const HidDetectionManager::InputMetadata& metadata) {
  return metadata.state == HidDetectionManager::InputState::kSearching ||
         metadata.state ==
             HidDetectionManager::InputState::kPairingViaBluetooth;
}

}  // namespace

FakeHidDetectionManager::FakeHidDetectionManager() = default;

FakeHidDetectionManager::~FakeHidDetectionManager() = default;

void FakeHidDetectionManager::SetHidStatusTouchscreenDetected(
    bool touchscreen_detected) {
  touchscreen_detected_ = touchscreen_detected;
  if (!is_hid_detection_active_)
    return;

  NotifyHidDetectionStatusChanged();
}

void FakeHidDetectionManager::SetHidStatusPointerMetadata(
    InputMetadata metadata) {
  pointer_metadata_ = metadata;
  if (!is_hid_detection_active_)
    return;

  NotifyHidDetectionStatusChanged();
}

void FakeHidDetectionManager::SetHidStatusKeyboardMetadata(
    InputMetadata metadata) {
  keyboard_metadata_ = metadata;
  if (!is_hid_detection_active_)
    return;

  NotifyHidDetectionStatusChanged();
}

void FakeHidDetectionManager::SetPairingState(
    std::optional<BluetoothHidPairingState> pairing_state) {
  pairing_state_ = std::move(pairing_state);
  NotifyHidDetectionStatusChanged();
}

base::WeakPtr<FakeHidDetectionManager> FakeHidDetectionManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void FakeHidDetectionManager::GetIsHidDetectionRequired(
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(IsInputMissing(pointer_metadata_) ||
                          IsInputMissing(keyboard_metadata_));
}

void FakeHidDetectionManager::PerformStartHidDetection() {
  DCHECK(!is_hid_detection_active_);
  is_hid_detection_active_ = true;
  NotifyHidDetectionStatusChanged();
}

void FakeHidDetectionManager::PerformStopHidDetection() {
  DCHECK(is_hid_detection_active_);
  is_hid_detection_active_ = false;
}

HidDetectionManager::HidDetectionStatus
FakeHidDetectionManager::ComputeHidDetectionStatus() const {
  std::optional<BluetoothHidPairingState> pairing_state;
  if (pairing_state_.has_value()) {
    pairing_state = BluetoothHidPairingState{
        pairing_state_.value().code, pairing_state_.value().num_keys_entered};
  }

  return HidDetectionStatus(pointer_metadata_, keyboard_metadata_,
                            touchscreen_detected_, std::move(pairing_state));
}

}  // namespace ash::hid_detection
