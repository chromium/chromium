// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_HID_DETECTION_HID_DETECTION_MANAGER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_HID_DETECTION_HID_DETECTION_MANAGER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/hid_detection/hid_detection_manager.h"

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/hid_detection/bluetooth_hid_detector.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/device_service.mojom.h"
#include "services/device/public/mojom/input_service.mojom.h"

namespace ash::hid_detection {

// Concrete HidDetectionManager implementation that uses InputDeviceManager and
// BluetoothHidDetectorImpl to detect and connect with devices.
class HidDetectionManagerImpl : public HidDetectionManager,
                                public device::mojom::InputDeviceManagerClient,
                                public BluetoothHidDetector::Delegate {
 public:
  using InputDeviceManagerBinder = base::RepeatingCallback<void(
      mojo::PendingReceiver<device::mojom::InputDeviceManager>)>;

  // Allows tests to override how this class binds InputDeviceManager receivers.
  static void SetInputDeviceManagerBinderForTest(
      InputDeviceManagerBinder binder);

  explicit HidDetectionManagerImpl(
      device::mojom::DeviceService* device_service);
  ~HidDetectionManagerImpl() override;

 private:
  friend class HidDetectionManagerImplTest;

  // HidDetectionManager:
  void GetIsHidDetectionRequired(
      base::OnceCallback<void(bool)> callback) override;
  void PerformStartHidDetection() override;
  void PerformStopHidDetection() override;
  HidDetectionManager::HidDetectionStatus ComputeHidDetectionStatus()
      const override;

  // device::mojom::InputDeviceManagerClient:
  void InputDeviceAdded(device::mojom::InputDeviceInfoPtr info) override;
  void InputDeviceRemoved(const std::string& id) override;

  // BluetoothHidDetector::Delegate:
  void OnBluetoothHidStatusChanged() override;

  void BindToInputDeviceManagerIfNeeded();

  // Processes the list of input devices fetched by GetIsHidDetectionRequired().
  // Invokes |callback| with whether HID detection is required or not. The
  // returned device list is not saved.
  void OnGetDevicesForIsRequired(
      base::OnceCallback<void(bool)> callback,
      std::vector<device::mojom::InputDeviceInfoPtr> devices);

  // Populates |device_id_to_device_map_| with the initial input devices
  // connected when HID starts and sets the connected HIDs.
  void OnGetDevicesAndSetClient(
      std::vector<device::mojom::InputDeviceInfoPtr> devices);

  // Iterates through |device_id_to_device_map_| and attempts to set each
  // entry as a connected HID. Returns true if any of the entries were set as
  // connected and false otherwise.
  bool SetConnectedHids();

  // Sets |device| as a connected HID if device is an applicable HID and no
  // device of the same type is already connected. Returns true if the device
  // was set as connected and false otherwise.
  bool AttemptSetDeviceAsConnectedHid(
      const device::mojom::InputDeviceInfo& device);

  // Computes an InputMetadata based on if an input device is connected or if a
  // Bluetooth device of the same type as |input_type| is pairing. A null
  // |connected_device_id| means no HID for that input is connected. If
  // |connected_device_id| is not null, it must be a key for an entry in
  // |device_id_to_device_map_|. A null |current_pairing_device| means no
  // Bluetooth device is pairing.
  InputMetadata GetInputMetadata(
      const std::optional<std::string>& connected_device_id,
      BluetoothHidDetector::BluetoothHidType input_type,
      const std::optional<BluetoothHidDetector::BluetoothHidMetadata>&
          current_pairing_device) const;

  // Informs |bluetooth_hid_detector_| what devices are missing.
  void SetInputDevicesStatus();

  // Allows tests to override the BluetoothHidDetector implementation used.
  void SetBluetoothHidDetectorForTest(
      std::unique_ptr<BluetoothHidDetector> bluetooth_hid_detector);

  std::map<std::string, device::mojom::InputDeviceInfoPtr>
      device_id_to_device_map_;
  std::optional<std::string> connected_touchscreen_id_;
  std::optional<std::string> connected_pointer_id_;
  std::optional<std::string> connected_keyboard_id_;

  raw_ptr<device::mojom::DeviceService> device_service_ = nullptr;
  std::unique_ptr<BluetoothHidDetector> bluetooth_hid_detector_;
  mojo::Remote<device::mojom::InputDeviceManager> input_device_manager_;
  mojo::AssociatedReceiver<device::mojom::InputDeviceManagerClient>
      input_device_manager_receiver_{this};

  base::WeakPtrFactory<HidDetectionManagerImpl> weak_ptr_factory_{this};
};

}  // namespace ash::hid_detection

#endif  // CHROMEOS_ASH_COMPONENTS_HID_DETECTION_HID_DETECTION_MANAGER_IMPL_H_
