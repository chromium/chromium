// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_HID_DETECTION_FAKE_HID_DETECTION_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_HID_DETECTION_FAKE_HID_DETECTION_MANAGER_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/hid_detection/hid_detection_manager.h"

namespace ash::hid_detection {

class FakeHidDetectionManager : public HidDetectionManager {
 public:
  FakeHidDetectionManager();
  ~FakeHidDetectionManager() override;

  // Mocks the HID detection status being updated.
  void SetHidStatusTouchscreenDetected(bool touchscreen_detected);
  void SetHidStatusPointerMetadata(InputMetadata metadata);
  void SetHidStatusKeyboardMetadata(InputMetadata metadata);
  void SetPairingState(std::optional<BluetoothHidPairingState> pairing_state);

  base::WeakPtr<FakeHidDetectionManager> GetWeakPtr();

  bool is_hid_detection_active() const { return is_hid_detection_active_; }

 private:
  // HidDetectionManager:
  void GetIsHidDetectionRequired(
      base::OnceCallback<void(bool)> callback) override;
  void PerformStartHidDetection() override;
  void PerformStopHidDetection() override;
  HidDetectionManager::HidDetectionStatus ComputeHidDetectionStatus()
      const override;

  bool is_hid_detection_active_ = false;

  InputMetadata pointer_metadata_;
  InputMetadata keyboard_metadata_;
  bool touchscreen_detected_ = false;
  std::optional<BluetoothHidPairingState> pairing_state_;

  base::WeakPtrFactory<FakeHidDetectionManager> weak_ptr_factory_{this};
};

}  // namespace ash::hid_detection

#endif  // CHROMEOS_ASH_COMPONENTS_HID_DETECTION_FAKE_HID_DETECTION_MANAGER_H_
