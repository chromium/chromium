// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_HID_DETECTION_HID_DETECTION_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_HID_DETECTION_HID_DETECTION_MANAGER_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/hid_detection/bluetooth_hid_detector.h"

namespace ash::hid_detection {

// Manages detecting and automatically connecting to human interface devices.
class HidDetectionManager {
 public:
  // The connection state of an input.
  enum class InputState {
    // No device is connected.
    kSearching,

    // A device is connected via USB.
    kConnectedViaUsb,

    // A device is being paired with via Bluetooth.
    kPairingViaBluetooth,

    // A device is connected via Bluetooth.
    kPairedViaBluetooth,

    // A device is connected, but is not known to be USB (Bluetooth vs USB vs
    // serial).
    kConnected
  };

  // Info of an input on the device.
  struct InputMetadata {
    InputState state = InputState::kSearching;

    // The name of the HID currently being interfaced with. Empty if |state| is
    // kSearching (no HID is being interfaced with).
    std::string detected_hid_name;
  };

  // Represents the status of inputs on the device.
  struct HidDetectionStatus {
    HidDetectionStatus(InputMetadata pointer_metadata,
                       InputMetadata keyboard_metadata,
                       bool touchscreen_detected,
                       std::optional<BluetoothHidPairingState> pairing_state);
    HidDetectionStatus(HidDetectionStatus&& other);
    HidDetectionStatus& operator=(HidDetectionStatus&& other);
    ~HidDetectionStatus();

    // Pointer input info of the device.
    InputMetadata pointer_metadata;

    // Keyboard input info of the device.
    InputMetadata keyboard_metadata;

    // Indicates the device has a touchscreen connected.
    bool touchscreen_detected = false;

    // Set if the current pairing requires a code that should be displayed to
    // the user to enter.
    std::optional<BluetoothHidPairingState> pairing_state;
  };

  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Invoked whenever any HID detection status property changes.
    virtual void OnHidDetectionStatusChanged(HidDetectionStatus status) = 0;
  };

  virtual ~HidDetectionManager();

  // Invokes |callback| with a result indicating whether HID detection is
  // required or not. If both a keyboard and pointer are connected, HID
  // detection is not required, otherwise it is.
  virtual void GetIsHidDetectionRequired(
      base::OnceCallback<void(bool)> callback) = 0;

  // Begins scanning for HIDs. Informs |delegate| every time
  // the status of HID detection changes. This should only be called once in the
  // lifetime of this class.
  void StartHidDetection(Delegate* delegate);

  // Stops scanning for HIDs. This should only be called while HID detection is
  // active.
  void StopHidDetection();

 protected:
  HidDetectionManager() = default;

  // Implementation-specific version of StartHidDetection().
  virtual void PerformStartHidDetection() = 0;

  // Implementation-specific version of StopHidDetection().
  virtual void PerformStopHidDetection() = 0;

  // Computes the HID detection status sent to |delegate_|.
  virtual HidDetectionStatus ComputeHidDetectionStatus() const = 0;

  // Notifies |delegate_| of status changes; should be called by derived
  // types to notify observers of status changes.
  void NotifyHidDetectionStatusChanged();

  raw_ptr<Delegate> delegate_ = nullptr;
};

}  // namespace ash::hid_detection

#endif  // CHROMEOS_ASH_COMPONENTS_HID_DETECTION_HID_DETECTION_MANAGER_H_
