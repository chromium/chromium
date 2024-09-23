// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/hid_detection/bluetooth_hid_detector_impl.h"

#include "ash/public/cpp/bluetooth_config_service.h"
#include "base/strings/utf_string_conversions.h"
#include "components/device_event_log/device_event_log.h"
#include "hid_detection_utils.h"

namespace ash::hid_detection {

namespace {

using bluetooth_config::mojom::BluetoothDevicePropertiesPtr;
using bluetooth_config::mojom::BluetoothSystemState;
using bluetooth_config::mojom::DeviceType;
using bluetooth_config::mojom::KeyEnteredHandler;

// Returns the BluetoothHidType corresponding with |device|'s device type, or
// std::nullopt if |device| is not a HID.
std::optional<BluetoothHidDetector::BluetoothHidType> GetBluetoothHidType(
    const BluetoothDevicePropertiesPtr& device) {
  switch (device->device_type) {
    case DeviceType::kMouse:
      [[fallthrough]];
    case DeviceType::kTablet:
      return BluetoothHidDetector::BluetoothHidType::kPointer;
    case DeviceType::kKeyboard:
      return BluetoothHidDetector::BluetoothHidType::kKeyboard;
    case DeviceType::kKeyboardMouseCombo:
      return BluetoothHidDetector::BluetoothHidType::kKeyboardPointerCombo;
    default:
      return std::nullopt;
  }
}

}  // namespace

BluetoothHidDetectorImpl::BluetoothHidDetectorImpl() = default;

BluetoothHidDetectorImpl::~BluetoothHidDetectorImpl() {
  DCHECK_EQ(kNotStarted, state_) << " HID detection must be stopped before "
                                 << "BluetoothHidDetectorImpl is destroyed.";
}

void BluetoothHidDetectorImpl::SetInputDevicesStatus(
    InputDevicesStatus input_devices_status) {
  HID_LOG(EVENT) << "Input devices status set, pointer missing: "
                 << input_devices_status.pointer_is_missing
                 << ", keyboard missing: "
                 << input_devices_status.keyboard_is_missing;
  input_devices_status_ = input_devices_status;

  if (!current_pairing_device_)
    return;

  std::optional<BluetoothHidDetector::BluetoothHidType>
      current_pairing_device_hid_type =
          GetBluetoothHidType(current_pairing_device_.value());
  DCHECK(current_pairing_device_hid_type)
      << current_pairing_device_.value()->id
      << " does not have a valid HID type, device type: "
      << current_pairing_device_.value()->device_type;

  if (IsHidTypeMissing(current_pairing_device_hid_type.value()))
    return;

  // If the HID type of |current_pairing_device_| is no longer missing, this can
  // mean 2 things:
  // 1. |current_pairing_device_| has successfully finished pairing, and the
  //    client of this class has now invoked this method to inform this class
  //    that the HID type is no longer missing. Clear the current pairing state
  //    and process the next device in the queue.
  // 2. |current_pairing_device_| is currently pairing, and a device of the
  //    same type has been detected as connected. Clear the current pairing
  //    state, which will cause the current pairing to cancel, and process the
  //    next device in the queue.
  HID_LOG(EVENT) << "Device type of "
                 << current_pairing_device_.value()->device_type << " for "
                 << current_pairing_device_.value()->id
                 << " is no longer missing";
  ClearCurrentPairingState();
}

const BluetoothHidDetector::BluetoothHidDetectionStatus
BluetoothHidDetectorImpl::GetBluetoothHidDetectionStatus() {
  if (!current_pairing_device_.has_value()) {
    return BluetoothHidDetectionStatus(
        /*current_pairing_device*/ std::nullopt,
        /*pairing_state=*/std::nullopt);
  }

  std::optional<BluetoothHidPairingState> pairing_state;
  if (current_pairing_state_.has_value()) {
    pairing_state = BluetoothHidPairingState{
        current_pairing_state_.value().code,
        current_pairing_state_.value().num_keys_entered};
  }

  std::optional<BluetoothHidType> hid_type =
      GetBluetoothHidType(current_pairing_device_.value());
  DCHECK(hid_type) << " |current_pairing_device_| has an invalid HID type";

  return BluetoothHidDetectionStatus{
      BluetoothHidMetadata{
          base::UTF16ToUTF8(current_pairing_device_.value()->public_name),
          hid_type.value()},
      std::move(pairing_state)};
}

void BluetoothHidDetectorImpl::PerformStartBluetoothHidDetection(
    InputDevicesStatus input_devices_status) {
  DCHECK_EQ(kNotStarted, state_);
  HID_LOG(EVENT) << "Starting Bluetooth HID detection, pointer missing: "
                 << input_devices_status.pointer_is_missing
                 << ", keyboard missing: "
                 << input_devices_status.keyboard_is_missing;
  input_devices_status_ = input_devices_status;
  state_ = kStarting;
  num_pairing_attempts_ = 0;
  GetBluetoothConfigService(
      cros_bluetooth_config_remote_.BindNewPipeAndPassReceiver());
  cros_bluetooth_config_remote_->ObserveSystemProperties(
      system_properties_observer_receiver_.BindNewPipeAndPassRemote());
}

void BluetoothHidDetectorImpl::PerformStopBluetoothHidDetection(
    bool is_using_bluetooth) {
  DCHECK_NE(kNotStarted, state_)
      << " Call to StopBluetoothHidDetection() while "
      << "HID detection is inactive.";
  HID_LOG(EVENT) << "Stopping Bluetooth HID detection, |is_using_bluetooth|: "
                 << is_using_bluetooth;
  hid_detection::RecordBluetoothPairingAttempts(num_pairing_attempts_);
  state_ = kNotStarted;
  cros_bluetooth_config_remote_->SetBluetoothHidDetectionInactive(
      is_using_bluetooth);
  cros_bluetooth_config_remote_.reset();
  system_properties_observer_receiver_.reset();
  ResetDiscoveryState();
}

void BluetoothHidDetectorImpl::OnPropertiesUpdated(
    bluetooth_config::mojom::BluetoothSystemPropertiesPtr properties) {
  switch (state_) {
    case kNotStarted:
      NOTREACHED_IN_MIGRATION()
          << "SystemPropertiesObserver should not be bound while in "
             "state |kNotStarted|";
      return;
    case kStarting:
      if (properties->system_state == BluetoothSystemState::kEnabled) {
        HID_LOG(EVENT)
            << "Bluetooth adapter is already enabled, starting discovery";
        state_ = kDetecting;
        cros_bluetooth_config_remote_->StartDiscovery(
            bluetooth_discovery_delegate_receiver_.BindNewPipeAndPassRemote());
      } else if (properties->system_state == BluetoothSystemState::kDisabled ||
                 properties->system_state == BluetoothSystemState::kDisabling) {
        HID_LOG(EVENT) << "Bluetooth adapter is disabled or disabling, "
                       << "enabling adapter";
        state_ = kEnablingAdapter;
        cros_bluetooth_config_remote_->SetBluetoothEnabledWithoutPersistence();
      } else {
        HID_LOG(EVENT)
            << "Bluetooth adapter is unavailable or enabling, waiting "
            << "for next state change";
      }
      return;
    case kEnablingAdapter:
      if (properties->system_state == BluetoothSystemState::kEnabled) {
        HID_LOG(EVENT)
            << "Bluetooth adapter has become enabled, starting discovery";
        state_ = kDetecting;
        cros_bluetooth_config_remote_->StartDiscovery(
            bluetooth_discovery_delegate_receiver_.BindNewPipeAndPassRemote());
      }
      return;
    case kDetecting:
      if (properties->system_state != BluetoothSystemState::kEnabled) {
        HID_LOG(EVENT) << "Bluetooth adapter has stopped being enabled while "
                       << "Bluetooth HID detection is in progress";
        state_ = kStoppedExternally;
      }
      return;
    case kStoppedExternally:
      if (properties->system_state == BluetoothSystemState::kEnabled) {
        HID_LOG(EVENT) << "Bluetooth adapter has become enabled after being "
                       << "unenabled externally, starting discovery";
        state_ = kDetecting;
        cros_bluetooth_config_remote_->StartDiscovery(
            bluetooth_discovery_delegate_receiver_.BindNewPipeAndPassRemote());
      }
      return;
  }
}

void BluetoothHidDetectorImpl::OnBluetoothDiscoveryStarted(
    mojo::PendingRemote<bluetooth_config::mojom::DevicePairingHandler>
        handler) {
  HID_LOG(EVENT) << "Bluetooth discovery started.";
  DCHECK(!device_pairing_handler_remote_);
  device_pairing_handler_remote_.Bind(std::move(handler));
}

void BluetoothHidDetectorImpl::OnBluetoothDiscoveryStopped() {
  HID_LOG(EVENT) << "Bluetooth discovery stopped.";
  ResetDiscoveryState();
}

void BluetoothHidDetectorImpl::OnDiscoveredDevicesListChanged(
    std::vector<BluetoothDevicePropertiesPtr> discovered_devices) {
  for (const auto& discovered_device : discovered_devices) {
    if (!ShouldAttemptToPairWithDevice(discovered_device))
      continue;
    if (queued_device_ids_.contains(discovered_device->id))
      continue;

    queued_device_ids_.insert(discovered_device->id);
    queue_->emplace(discovered_device.Clone());
    HID_LOG(EVENT) << "Queuing device: " << discovered_device->id << ". ["
                   << queue_->size() << "] devices now in queue.";
  }
  ProcessQueue();
}

void BluetoothHidDetectorImpl::RequestPinCode(RequestPinCodeCallback callback) {
  DCHECK(current_pairing_device_)
      << "RequestPinCode() called with no |current_pairing_device_|";

  // RequestPinCode auth is not attributed to HIDs, cancel the pairing.
  HID_LOG(EVENT) << "RequestPinCode auth required for "
                 << current_pairing_device_.value()->id
                 << ", cancelling pairing";
  ClearCurrentPairingState();
}

void BluetoothHidDetectorImpl::RequestPasskey(RequestPasskeyCallback callback) {
  DCHECK(current_pairing_device_)
      << "RequestPasskey() called with no |current_pairing_device_|";

  // RequestPasskey auth is not attributed to HIDs, cancel the pairing.
  HID_LOG(EVENT) << "RequestPasskey auth required for "
                 << current_pairing_device_.value()->id
                 << ", cancelling pairing";
  ClearCurrentPairingState();
}

void BluetoothHidDetectorImpl::DisplayPinCode(
    const std::string& pin_code,
    mojo::PendingReceiver<KeyEnteredHandler> handler) {
  DCHECK(current_pairing_device_)
      << "DisplayPinCode() called with no |current_pairing_device_|";
  HID_LOG(EVENT) << "DisplayPinCode auth required for "
                 << current_pairing_device_.value()->id
                 << ", pin code: " << pin_code;
  RequirePairingCode(pin_code, std::move(handler));
}

void BluetoothHidDetectorImpl::DisplayPasskey(
    const std::string& passkey,
    mojo::PendingReceiver<KeyEnteredHandler> handler) {
  DCHECK(current_pairing_device_)
      << "DisplayPasskey() called with no |current_pairing_device_|";
  HID_LOG(EVENT) << "DisplayPasskey auth required for "
                 << current_pairing_device_.value()->id
                 << ", passkey: " << passkey;
  RequirePairingCode(passkey, std::move(handler));
}

void BluetoothHidDetectorImpl::ConfirmPasskey(const std::string& passkey,
                                              ConfirmPasskeyCallback callback) {
  DCHECK(current_pairing_device_)
      << "ConfirmPasskey() called with no |current_pairing_device_|";

  // ConfirmPasskey auth is not attributed to HIDs, cancel the pairing.
  HID_LOG(EVENT) << "ConfirmPasskey auth required for "
                 << current_pairing_device_.value()->id
                 << ", cancelling pairing";
  ClearCurrentPairingState();
}

void BluetoothHidDetectorImpl::AuthorizePairing(
    AuthorizePairingCallback callback) {
  DCHECK(current_pairing_device_)
      << "AuthorizePairing() called with no |current_pairing_device_|";
  HID_LOG(EVENT) << "AuthorizePairing auth required for "
                 << current_pairing_device_.value()->id
                 << ", automatically authorizing";
  std::move(callback).Run(/*confirmed=*/true);
}

void BluetoothHidDetectorImpl::HandleKeyEntered(uint8_t num_keys_entered) {
  DCHECK(current_pairing_device_)
      << "HandleKeyEntered() called with no |current_pairing_device_|";
  DCHECK(current_pairing_state_)
      << "HandleKeyEntered() called with no |current_pairing_state_|";

  HID_LOG(EVENT) << "HandleKeyEntered called with "
                 << static_cast<unsigned>(num_keys_entered) << " keys entered";
  current_pairing_state_->num_keys_entered = num_keys_entered;
  NotifyBluetoothHidDetectionStatusChanged();
}

bool BluetoothHidDetectorImpl::IsHidTypeMissing(
    BluetoothHidDetector::BluetoothHidType hid_type) {
  switch (hid_type) {
    case BluetoothHidDetector::BluetoothHidType::kPointer:
      return input_devices_status_.pointer_is_missing;
    case BluetoothHidDetector::BluetoothHidType::kKeyboard:
      return input_devices_status_.keyboard_is_missing;
    case BluetoothHidDetector::BluetoothHidType::kKeyboardPointerCombo:
      return input_devices_status_.pointer_is_missing ||
             input_devices_status_.keyboard_is_missing;
  }
}

bool BluetoothHidDetectorImpl::ShouldAttemptToPairWithDevice(
    const BluetoothDevicePropertiesPtr& device) {
  std::optional<BluetoothHidDetector::BluetoothHidType> hid_type =
      GetBluetoothHidType(device);
  if (!hid_type)
    return false;

  return IsHidTypeMissing(hid_type.value());
}

void BluetoothHidDetectorImpl::ProcessQueue() {
  if (current_pairing_device_)
    return;

  if (queue_->empty()) {
    HID_LOG(DEBUG) << "No devices queued";
    return;
  }

  current_pairing_device_ = std::move(queue_->front());
  queue_->pop();
  HID_LOG(EVENT) << "Popped device with id: "
                 << current_pairing_device_.value()->id
                 << " from front of queue. [" << queue_->size()
                 << "] devices now in queue.";

  if (!ShouldAttemptToPairWithDevice(current_pairing_device_.value())) {
    HID_LOG(EVENT) << "Device with id " << current_pairing_device_.value()->id
                   << " no longer should be attempted to be paired with, "
                   << "processing next device in queue. Device type: "
                   << current_pairing_device_.value()->device_type;
    current_pairing_device_.reset();
    ProcessQueue();
    return;
  }

  HID_LOG(EVENT) << "Pairing with device with id: "
                 << current_pairing_device_.value()->id;
  ++num_pairing_attempts_;

  // Start a timer to make sure that the queue never gets stuck, such as in
  // b/242358619.
  current_pairing_timer_.Start(
      FROM_HERE, kMaxPairingSessionDuration,
      base::BindOnce(&BluetoothHidDetectorImpl::OnPairingTimeout,
                     weak_ptr_factory_.GetWeakPtr()));

  device_pairing_handler_remote_->PairDevice(
      current_pairing_device_.value()->id,
      device_pairing_delegate_receiver_.BindNewPipeAndPassRemote(),
      base::BindOnce(&BluetoothHidDetectorImpl::OnPairDevice,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::make_unique<base::ElapsedTimer>()));
  NotifyBluetoothHidDetectionStatusChanged();
}

void BluetoothHidDetectorImpl::OnPairDevice(
    std::unique_ptr<base::ElapsedTimer> metrics_timer,
    bluetooth_config::mojom::PairingResult pairing_result) {
  DCHECK(current_pairing_device_)
      << "OnPairDevice() called with no |current_pairing_device_|";

  HID_LOG(EVENT) << "Finished pairing with "
                 << current_pairing_device_.value()->id
                 << ", result: " << pairing_result << ", [" << queue_->size()
                 << "] devices still in queue.";

  const bool success =
      pairing_result == bluetooth_config::mojom::PairingResult::kSuccess;
  hid_detection::RecordBluetoothPairingResult(success,
                                              metrics_timer->Elapsed());

  // If pairing has succeeded, wait for SetInputDevicesStatus() to be called
  // with the corresponding HID type no longer missing.
  if (success) {
    HID_LOG(EVENT)
        << "Pairing succeeded, waiting for input devices status to update.";
    return;
  }

  HID_LOG(ERROR) << "Pairing failed, clearing current pairing state and "
                 << "processing the next device in queue.";
  ClearCurrentPairingState();
}

void BluetoothHidDetectorImpl::OnPairingTimeout() {
  HID_LOG(ERROR) << "Pairing session has timed out, clearing current pairing "
                 << "state.";
  hid_detection::RecordPairingTimeoutExceeded();
  ClearCurrentPairingState();
}

void BluetoothHidDetectorImpl::ClearCurrentPairingState() {
  // If there is an ongoing pairing, it will be cancelled. Invalidate the
  // pairing finished callback. This will also invalidate the
  // |current_pairing_timer_| callback.
  weak_ptr_factory_.InvalidateWeakPtrs();

  queued_device_ids_.erase(current_pairing_device_.value()->id);
  current_pairing_device_.reset();
  current_pairing_state_.reset();
  device_pairing_delegate_receiver_.reset();
  key_entered_handler_receiver_.reset();
  NotifyBluetoothHidDetectionStatusChanged();
  ProcessQueue();
}

void BluetoothHidDetectorImpl::ResetDiscoveryState() {
  // Reset Mojo-related properties.
  bluetooth_discovery_delegate_receiver_.reset();
  device_pairing_handler_remote_.reset();
  device_pairing_delegate_receiver_.reset();
  key_entered_handler_receiver_.reset();

  // Reset queue-related properties.
  current_pairing_device_.reset();
  current_pairing_state_.reset();
  current_pairing_timer_.Stop();
  queue_ = std::make_unique<base::queue<BluetoothDevicePropertiesPtr>>();
  queued_device_ids_.clear();

  // Inform the client that no device is currently pairing.
  NotifyBluetoothHidDetectionStatusChanged();
}

void BluetoothHidDetectorImpl::RequirePairingCode(
    const std::string& code,
    mojo::PendingReceiver<KeyEnteredHandler> handler) {
  DCHECK(!current_pairing_state_) << "RequirePairingCode() called "
                                  << "with |current_pairing_state_| already "
                                  << "initialized";
  DCHECK(!key_entered_handler_receiver_.is_bound());
  key_entered_handler_receiver_.Bind(std::move(handler));
  current_pairing_state_ =
      BluetoothHidPairingState{code, /*num_keys_entered=*/0u};
  NotifyBluetoothHidDetectionStatusChanged();
}

}  // namespace ash::hid_detection
