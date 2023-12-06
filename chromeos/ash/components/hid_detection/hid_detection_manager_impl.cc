// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/hid_detection/hid_detection_manager_impl.h"

#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "chromeos/ash/components/hid_detection/bluetooth_hid_detector_impl.h"
#include "chromeos/ash/components/hid_detection/hid_detection_utils.h"
#include "components/device_event_log/device_event_log.h"

namespace ash::hid_detection {
namespace {
using BluetoothHidType = BluetoothHidDetector::BluetoothHidType;
using InputState = HidDetectionManager::InputState;

// In floss, a virtual device is created when a HID is bonded or paired.
// We do not want to include this virtual device to our list of added devices.
// (b/299955128)
const char* kBlockedDeviceNames[] = {"VIRTUAL_SUSPEND_UHID"};

HidDetectionManagerImpl::InputDeviceManagerBinder&
GetInputDeviceManagerBinderOverride() {
  // InputDeviceManagerBinder instance that can be overridden in tests.
  static base::NoDestructor<HidDetectionManagerImpl::InputDeviceManagerBinder>
      binder;
  return *binder;
}

}  // namespace

// static
void HidDetectionManagerImpl::SetInputDeviceManagerBinderForTest(
    InputDeviceManagerBinder binder) {
  GetInputDeviceManagerBinderOverride() = std::move(binder);
}

HidDetectionManagerImpl::HidDetectionManagerImpl(
    device::mojom::DeviceService* device_service)
    : device_service_{device_service},
      bluetooth_hid_detector_{std::make_unique<BluetoothHidDetectorImpl>()} {}

HidDetectionManagerImpl::~HidDetectionManagerImpl() = default;

void HidDetectionManagerImpl::GetIsHidDetectionRequired(
    base::OnceCallback<void(bool)> callback) {
  BindToInputDeviceManagerIfNeeded();

  HID_LOG(EVENT) << "Fetching input devices for GetIsHidDetectionRequired().";
  input_device_manager_->GetDevices(
      base::BindOnce(&HidDetectionManagerImpl::OnGetDevicesForIsRequired,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void HidDetectionManagerImpl::PerformStartHidDetection() {
  BindToInputDeviceManagerIfNeeded();

  HID_LOG(EVENT) << "Starting HID detection by fetching input devices.";
  input_device_manager_->GetDevicesAndSetClient(
      input_device_manager_receiver_.BindNewEndpointAndPassRemote(),
      base::BindOnce(&HidDetectionManagerImpl::OnGetDevicesAndSetClient,
                     weak_ptr_factory_.GetWeakPtr()));
}

void HidDetectionManagerImpl::PerformStopHidDetection() {
  HID_LOG(EVENT) << "Stopping HID detection.";
  input_device_manager_receiver_.reset();

  // Check if any of the connected input devices are connected via Bluetooth.
  bool is_using_bluetooth = false;
  for (const auto& [device_id, device] : device_id_to_device_map_) {
    if (device->type == device::mojom::InputDeviceType::TYPE_BLUETOOTH) {
      is_using_bluetooth = true;
      break;
    }
  }
  bluetooth_hid_detector_->StopBluetoothHidDetection(is_using_bluetooth);
}

HidDetectionManager::HidDetectionStatus
HidDetectionManagerImpl::ComputeHidDetectionStatus() const {
  BluetoothHidDetector::BluetoothHidDetectionStatus bluetooth_status =
      bluetooth_hid_detector_->GetBluetoothHidDetectionStatus();
  return HidDetectionManager::HidDetectionStatus(
      GetInputMetadata(connected_pointer_id_, BluetoothHidType::kPointer,
                       bluetooth_status.current_pairing_device),
      GetInputMetadata(connected_keyboard_id_, BluetoothHidType::kKeyboard,
                       bluetooth_status.current_pairing_device),
      connected_touchscreen_id_.has_value(),
      std::move(bluetooth_status.pairing_state));
}

void HidDetectionManagerImpl::InputDeviceAdded(
    device::mojom::InputDeviceInfoPtr info) {
  // Special case where the added device is a blocked device.
  if (std::find(std::begin(kBlockedDeviceNames), std::end(kBlockedDeviceNames),
                info->name) != std::end(kBlockedDeviceNames)) {
    return;
  }

  HID_LOG(EVENT) << "Input device added, id: " << info->id
                 << ", name: " << info->name;
  const std::string& device_id = info->id;
  device_id_to_device_map_[device_id] = std::move(info);
  hid_detection::RecordHidConnected(*device_id_to_device_map_[device_id]);

  if (AttemptSetDeviceAsConnectedHid(*device_id_to_device_map_[device_id])) {
    NotifyHidDetectionStatusChanged();
    SetInputDevicesStatus();
  }
}

void HidDetectionManagerImpl::InputDeviceRemoved(const std::string& id) {
  if (!base::Contains(device_id_to_device_map_, id)) {
    // Some devices may be removed that were not registered in
    // InputDeviceAdded() or OnGetDevicesAndSetClient().
    HID_LOG(EVENT)
        << "Input device with id: " << id
        << " was removed that was not in |device_id_to_device_map_|.";
    return;
  }

  HID_LOG(EVENT) << "Input device removed, id: " << id
                 << ", name: " << device_id_to_device_map_[id]->name;
  hid_detection::RecordHidDisconnected(*device_id_to_device_map_[id]);
  device_id_to_device_map_.erase(id);
  bool was_connected_hid_disconnected_ = false;

  if (id == connected_touchscreen_id_) {
    HID_LOG(EVENT) << "Removing connected touchscreen: " << id;
    connected_touchscreen_id_.reset();
    was_connected_hid_disconnected_ = true;
  }
  if (id == connected_pointer_id_) {
    HID_LOG(EVENT) << "Removing connected pointer: " << id;
    connected_pointer_id_.reset();
    was_connected_hid_disconnected_ = true;
  }
  if (id == connected_keyboard_id_) {
    HID_LOG(EVENT) << "Removing connected keyboard: " << id;
    connected_keyboard_id_.reset();
    was_connected_hid_disconnected_ = true;
  }

  if (was_connected_hid_disconnected_) {
    SetConnectedHids();
    NotifyHidDetectionStatusChanged();
    SetInputDevicesStatus();
  }
}

void HidDetectionManagerImpl::OnBluetoothHidStatusChanged() {
  NotifyHidDetectionStatusChanged();
}

void HidDetectionManagerImpl::BindToInputDeviceManagerIfNeeded() {
  if (input_device_manager_.is_bound())
    return;

  mojo::PendingReceiver<device::mojom::InputDeviceManager> receiver =
      input_device_manager_.BindNewPipeAndPassReceiver();
  const auto& binder = GetInputDeviceManagerBinderOverride();
  if (binder) {
    binder.Run(std::move(receiver));
    return;
  }

  DCHECK(device_service_);
  device_service_->BindInputDeviceManager(std::move(receiver));
}

void HidDetectionManagerImpl::OnGetDevicesForIsRequired(
    base::OnceCallback<void(bool)> callback,
    std::vector<device::mojom::InputDeviceInfoPtr> devices) {
  bool has_pointer = false;
  bool has_keyboard = false;
  for (const auto& device : devices) {
    if (hid_detection::IsDevicePointer(*device))
      has_pointer = true;

    if (device->is_keyboard)
      has_keyboard = true;

    if (has_pointer && has_keyboard)
      break;
  }

  hid_detection::HidsMissing hids_missing = hid_detection::HidsMissing::kNone;
  if (!has_pointer) {
    if (!has_keyboard) {
      hids_missing = hid_detection::HidsMissing::kPointerAndKeyboard;
    } else {
      hids_missing = hid_detection::HidsMissing::kPointer;
    }
  } else if (!has_keyboard) {
    hids_missing = hid_detection::HidsMissing::kKeyboard;
  }
  hid_detection::RecordInitialHidsMissing(hids_missing);

  HID_LOG(EVENT)
      << "Fetched " << devices.size()
      << " input devices for GetIsHidDetectionRequired(). Pointer detected: "
      << has_pointer << ", keyboard detected: " << has_keyboard;

  // HID detection is not required if both devices are present.
  std::move(callback).Run(!(has_pointer && has_keyboard));
}

void HidDetectionManagerImpl::OnGetDevicesAndSetClient(
    std::vector<device::mojom::InputDeviceInfoPtr> devices) {
  DCHECK(device_id_to_device_map_.empty())
      << " |devices_| should be empty when fetching initial devices.";
  for (auto& device : devices) {
    device_id_to_device_map_[device->id] = std::move(device);
  }
  SetConnectedHids();
  NotifyHidDetectionStatusChanged();

  bluetooth_hid_detector_->StartBluetoothHidDetection(
      this, {.pointer_is_missing = !connected_pointer_id_.has_value(),
             .keyboard_is_missing = !connected_keyboard_id_.has_value()});
}

bool HidDetectionManagerImpl::SetConnectedHids() {
  HID_LOG(EVENT) << "Setting connected HIDs";
  bool is_any_device_newly_connected_hid = false;
  for (const auto& [device_id, device] : device_id_to_device_map_) {
    is_any_device_newly_connected_hid |=
        AttemptSetDeviceAsConnectedHid(*device);
  }
  return is_any_device_newly_connected_hid;
}

bool HidDetectionManagerImpl::AttemptSetDeviceAsConnectedHid(
    const device::mojom::InputDeviceInfo& device) {
  bool is_device_newly_connected_hid = false;
  if (!connected_touchscreen_id_.has_value() &&
      hid_detection::IsDeviceTouchscreen(device)) {
    HID_LOG(EVENT) << "Touchscreen detected: " << device.id;
    connected_touchscreen_id_ = device.id;
    is_device_newly_connected_hid = true;
  }
  if (!connected_pointer_id_.has_value() &&
      hid_detection::IsDevicePointer(device)) {
    HID_LOG(EVENT) << "Pointer detected: " << device.id;
    connected_pointer_id_ = device.id;
    is_device_newly_connected_hid = true;
  }
  if (!connected_keyboard_id_.has_value() && device.is_keyboard) {
    HID_LOG(EVENT) << "Keyboard detected: " << device.id;
    connected_keyboard_id_ = device.id;
    is_device_newly_connected_hid = true;
  }

  return is_device_newly_connected_hid;
}

HidDetectionManager::InputMetadata HidDetectionManagerImpl::GetInputMetadata(
    const std::optional<std::string>& connected_device_id,
    BluetoothHidType input_type,
    const std::optional<BluetoothHidDetector::BluetoothHidMetadata>&
        current_pairing_device) const {
  if (connected_device_id.has_value()) {
    const device::mojom::InputDeviceInfoPtr& device =
        device_id_to_device_map_.find(connected_device_id.value())->second;
    DCHECK(device)
        << " |connected_device_id| not found in |device_id_to_device_map_|";
    InputState state;
    switch (device->type) {
      case device::mojom::InputDeviceType::TYPE_BLUETOOTH:
        state = InputState::kPairedViaBluetooth;
        break;
      case device::mojom::InputDeviceType::TYPE_USB:
        state = InputState::kConnectedViaUsb;
        break;
      case device::mojom::InputDeviceType::TYPE_SERIO:
        [[fallthrough]];
      case device::mojom::InputDeviceType::TYPE_UNKNOWN:
        state = InputState::kConnected;
        break;
    }
    return InputMetadata{state, device->name};
  }

  if (current_pairing_device.has_value() &&
      (current_pairing_device.value().type == input_type ||
       current_pairing_device.value().type ==
           BluetoothHidType::kKeyboardPointerCombo)) {
    return InputMetadata{InputState::kPairingViaBluetooth,
                         current_pairing_device.value().name};
  }

  return InputMetadata();
}

void HidDetectionManagerImpl::SetInputDevicesStatus() {
  bluetooth_hid_detector_->SetInputDevicesStatus(
      {.pointer_is_missing = !connected_pointer_id_.has_value(),
       .keyboard_is_missing = !connected_keyboard_id_.has_value()});
}

void HidDetectionManagerImpl::SetBluetoothHidDetectorForTest(
    std::unique_ptr<BluetoothHidDetector> bluetooth_hid_detector) {
  bluetooth_hid_detector_ = std::move(bluetooth_hid_detector);
}

}  // namespace ash::hid_detection
