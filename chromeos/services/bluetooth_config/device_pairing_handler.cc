// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/device_pairing_handler.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "components/device_event_log/device_event_log.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/chromeos/bluetooth_utils.h"

namespace chromeos {
namespace bluetooth_config {

namespace {

// The number of digits a passkey used for pairing will be. This is documented
// in device::BluetoothDevice.
const size_t kNumPasskeyDigits = 6;

// Converts |passkey| to a kNumPasskeyDigits-digit string, padding with zeroes
// if necessary.
std::string PasskeyToString(uint32_t passkey) {
  std::string passkey_string = base::NumberToString(passkey);

  // |passkey_string| should never be more than kNumPasskeyDigits digits long.
  DCHECK(passkey_string.length() <= kNumPasskeyDigits);
  return base::StrCat(
      {std::string(kNumPasskeyDigits - passkey_string.length(), '0'),
       passkey_string});
}

device::BluetoothTransport GetBluetoothTransport(
    device::BluetoothTransport type) {
  switch (type) {
    case device::BLUETOOTH_TRANSPORT_CLASSIC:
      return device::BLUETOOTH_TRANSPORT_CLASSIC;
    case device::BLUETOOTH_TRANSPORT_LE:
      return device::BLUETOOTH_TRANSPORT_LE;
    case device::BLUETOOTH_TRANSPORT_DUAL:
      return device::BLUETOOTH_TRANSPORT_DUAL;
    default:
      return device::BLUETOOTH_TRANSPORT_INVALID;
  }
}

mojom::PairingResult GetPairingResult(
    absl::optional<device::ConnectionFailureReason> failure_reason) {
  if (!failure_reason) {
    return mojom::PairingResult::kSuccess;
  }

  switch (failure_reason.value()) {
    case device::ConnectionFailureReason::kAuthTimeout:
      [[fallthrough]];
    case device::ConnectionFailureReason::kAuthFailed:
      return mojom::PairingResult::kAuthFailed;

    case device::ConnectionFailureReason::kUnknownError:
      [[fallthrough]];
    case device::ConnectionFailureReason::kSystemError:
      [[fallthrough]];
    case device::ConnectionFailureReason::kFailed:
      [[fallthrough]];
    case device::ConnectionFailureReason::kUnknownConnectionError:
      [[fallthrough]];
    case device::ConnectionFailureReason::kUnsupportedDevice:
      [[fallthrough]];
    case device::ConnectionFailureReason::kNotConnectable:
      return mojom::PairingResult::kNonAuthFailure;
  }
}

}  // namespace

// static
const base::TimeDelta DevicePairingHandler::kPairingFailureDelay =
    base::Milliseconds(500);

DevicePairingHandler::DevicePairingHandler(
    mojo::PendingReceiver<mojom::DevicePairingHandler> pending_receiver,
    AdapterStateController* adapter_state_controller,
    base::OnceClosure finished_pairing_callback)
    : adapter_state_controller_(adapter_state_controller),
      finished_pairing_callback_(std::move(finished_pairing_callback)) {
  adapter_state_controller_observation_.Observe(adapter_state_controller_);
  receiver_.Bind(std::move(pending_receiver));
}

DevicePairingHandler::~DevicePairingHandler() = default;

void DevicePairingHandler::CancelPairing() {
  is_canceling_pairing_ = true;
  device::BluetoothDevice* device = FindDevice(current_pairing_device_id_);
  if (!device) {
    BLUETOOTH_LOG(ERROR)
        << "Could not cancel pairing for device to due device no longer being "
           "found, identifier: "
        << current_pairing_device_id_;
    FinishCurrentPairingRequest(device::ConnectionFailureReason::kAuthFailed);
    return;
  }
  device->CancelPairing();
}

void DevicePairingHandler::NotifyFinished() {
  // |finished_pairing_callback_| can be null if we already succeeded from
  // pairing or the delegate disconnected, and now this handler is being
  // deleted.
  if (finished_pairing_callback_.is_null())
    return;
  std::move(finished_pairing_callback_).Run();
}

void DevicePairingHandler::PairDevice(
    const std::string& device_id,
    mojo::PendingRemote<mojom::DevicePairingDelegate> delegate,
    PairDeviceCallback callback) {
  BLUETOOTH_LOG(USER) << "Attempting to pair with device " << device_id;

  // There should only be one PairDevice request at a time.
  CHECK(current_pairing_device_id_.empty());

  pairing_start_timestamp_ = base::Time::Now();
  pair_device_callback_ = std::move(callback);

  delegate_.reset();
  delegate_.Bind(std::move(delegate));
  delegate_.set_disconnect_handler(base::BindOnce(
      &DevicePairingHandler::OnDelegateDisconnect, base::Unretained(this)));

  // If Bluetooth is not enabled, fail immediately.
  if (!IsBluetoothEnabled()) {
    BLUETOOTH_LOG(ERROR) << "Pairing failed due to Bluetooth not being "
                         << "enabled, device identifier: " << device_id;
    FinishCurrentPairingRequest(device::ConnectionFailureReason::kFailed);
    return;
  }

  // Find the device and attempt to pair to it.
  device::BluetoothDevice* device = FindDevice(device_id);

  if (!device) {
    BLUETOOTH_LOG(ERROR) << "Pairing failed due to device not being "
                         << "found, identifier: " << device_id;
    FinishCurrentPairingRequest(device::ConnectionFailureReason::kFailed);
    return;
  }

  current_pairing_device_id_ = device_id;
  device->Connect(
      /*delegate=*/this, base::BindOnce(&DevicePairingHandler::OnDeviceConnect,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void DevicePairingHandler::RequestPinCode(device::BluetoothDevice* device) {
  BLUETOOTH_LOG(EVENT) << "Requesting pin code for "
                       << current_pairing_device_id_;
  DCHECK(device->GetIdentifier() == current_pairing_device_id_);
  delegate_->RequestPinCode(base::BindOnce(
      &DevicePairingHandler::OnRequestPinCode, weak_ptr_factory_.GetWeakPtr()));
}

void DevicePairingHandler::RequestPasskey(device::BluetoothDevice* device) {
  BLUETOOTH_LOG(EVENT) << "Requesting passkey for "
                       << current_pairing_device_id_;
  DCHECK(device->GetIdentifier() == current_pairing_device_id_);
  delegate_->RequestPasskey(base::BindOnce(
      &DevicePairingHandler::OnRequestPasskey, weak_ptr_factory_.GetWeakPtr()));
}

void DevicePairingHandler::DisplayPinCode(device::BluetoothDevice* device,
                                          const std::string& pin_code) {
  BLUETOOTH_LOG(EVENT) << "Displaying pin code for "
                       << current_pairing_device_id_
                       << ", pin code: " << pin_code;
  DCHECK(device->GetIdentifier() == current_pairing_device_id_);
  key_entered_handler_.reset();
  delegate_->DisplayPinCode(pin_code,
                            key_entered_handler_.BindNewPipeAndPassReceiver());
}

void DevicePairingHandler::DisplayPasskey(device::BluetoothDevice* device,
                                          uint32_t passkey) {
  BLUETOOTH_LOG(EVENT) << "Displaying passkey for "
                       << current_pairing_device_id_
                       << ", passkey: " << passkey;
  DCHECK(device->GetIdentifier() == current_pairing_device_id_);
  key_entered_handler_.reset();
  delegate_->DisplayPasskey(PasskeyToString(passkey),
                            key_entered_handler_.BindNewPipeAndPassReceiver());
}

void DevicePairingHandler::KeysEntered(device::BluetoothDevice* device,
                                       uint32_t entered) {
  BLUETOOTH_LOG(EVENT) << entered << " keys entered for "
                       << current_pairing_device_id_;
  DCHECK(device->GetIdentifier() == current_pairing_device_id_);
  key_entered_handler_->HandleKeyEntered(entered);
}

void DevicePairingHandler::ConfirmPasskey(device::BluetoothDevice* device,
                                          uint32_t passkey) {
  BLUETOOTH_LOG(EVENT) << "Confirming passkey for "
                       << current_pairing_device_id_
                       << ", passkey: " << passkey;
  DCHECK(device->GetIdentifier() == current_pairing_device_id_);
  delegate_->ConfirmPasskey(
      PasskeyToString(passkey),
      base::BindOnce(&DevicePairingHandler::OnConfirmPairing,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DevicePairingHandler::AuthorizePairing(device::BluetoothDevice* device) {
  BLUETOOTH_LOG(EVENT) << "Authorizing pairing for "
                       << current_pairing_device_id_;
  DCHECK(device->GetIdentifier() == current_pairing_device_id_);
  delegate_->AuthorizePairing(base::BindOnce(
      &DevicePairingHandler::OnConfirmPairing, weak_ptr_factory_.GetWeakPtr()));
}

void DevicePairingHandler::OnAdapterStateChanged() {
  if (IsBluetoothEnabled())
    return;

  if (current_pairing_device_id_.empty())
    return;

  // If Bluetooth disables while we are attempting to pair, cancel the pairing.
  BLUETOOTH_LOG(EVENT) << "Bluetooth disabled while attempting to pair with "
                       << current_pairing_device_id_ << ", canceling pairing";
  CancelPairing();
}

void DevicePairingHandler::OnDeviceConnect(
    absl::optional<device::BluetoothDevice::ConnectErrorCode> error_code) {
  if (!error_code.has_value()) {
    BLUETOOTH_LOG(EVENT) << "Device " << current_pairing_device_id_
                         << " successfully paired";
    FinishCurrentPairingRequest(absl::nullopt);
    NotifyFinished();
    return;
  }

  // In some cases, device->Connect() will return a failure if the pairing
  // succeeded but the subsequent connection request returns with a failure.
  // Empirically, it's found that the device actually does connect, and
  // device->IsConnected() returns true. Wait |kPairingFailureDelay| to check if
  // the device is connected. Only do this if the failure is not due to a
  // pairing cancellation. If the pairing is canceled, we know for sure that the
  // device is not actually paired.
  // TODO(b/209531279): Remove this delay and |is_canceling_pairing| when the
  // root cause of issue is fixed.
  if (!is_canceling_pairing_) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DevicePairingHandler::HandlePairingFailed,
                       weak_ptr_factory_.GetWeakPtr(), error_code.value()),
        kPairingFailureDelay);
    return;
  }

  // Immediately handle pairing failures if pairing is being canceled, because
  // we know for sure that the device is not actually paired, and because if
  // the pairing is being canceled due to the handler being destroyed, if there
  // is a delay the failure will never be handled.
  HandlePairingFailed(error_code.value());
}

void DevicePairingHandler::HandlePairingFailed(
    device::BluetoothDevice::ConnectErrorCode error_code) {
  device::BluetoothDevice* device = FindDevice(current_pairing_device_id_);

  // In some cases, device->Connect() will return a failure if the pairing
  // succeeded but the subsequent connection request returns with a failure.
  // Empirically, it's found that the device actually does connect, and
  // device->IsConnected() returns true. Handle this case the
  // same as pairing succeeding if this wasn't a pairing cancellation.
  // TODO(b/209531279): Remove this when the root cause of issue is fixed.
  if (device && device->IsConnected() && !is_canceling_pairing_) {
    BLUETOOTH_LOG(EVENT)
        << device->GetAddress()
        << ": Pairing finished with an error code, but device "
        << "is connected. Handling like pairing succeeded. Error code: "
        << error_code;
    FinishCurrentPairingRequest(absl::nullopt);
    NotifyFinished();
    return;
  }

  // We use |current_pairing_device_id_| since it conveys the same information
  // as the address and |device| could be |nullptr|.
  BLUETOOTH_LOG(ERROR) << current_pairing_device_id_
                       << ": Pairing failed with error code: " << error_code;

  using ErrorCode = device::BluetoothDevice::ConnectErrorCode;
  switch (error_code) {
    case ErrorCode::ERROR_AUTH_CANCELED:
      [[fallthrough]];
    case ErrorCode::ERROR_AUTH_FAILED:
      [[fallthrough]];
    case ErrorCode::ERROR_AUTH_REJECTED:
      FinishCurrentPairingRequest(device::ConnectionFailureReason::kAuthFailed);
      return;
    case ErrorCode::ERROR_AUTH_TIMEOUT:
      FinishCurrentPairingRequest(
          device::ConnectionFailureReason::kAuthTimeout);
      return;

    case ErrorCode::ERROR_FAILED:
      [[fallthrough]];
    case ErrorCode::ERROR_INPROGRESS:
      FinishCurrentPairingRequest(device::ConnectionFailureReason::kFailed);
      return;
    case ErrorCode::ERROR_UNKNOWN:
      FinishCurrentPairingRequest(
          device::ConnectionFailureReason::kUnknownError);
      return;
    case ErrorCode::ERROR_UNSUPPORTED_DEVICE:
      FinishCurrentPairingRequest(
          device::ConnectionFailureReason::kUnsupportedDevice);
      return;
    default:
      BLUETOOTH_LOG(ERROR) << "Error code is invalid.";
      break;
  }
}

void DevicePairingHandler::OnRequestPinCode(const std::string& pin_code) {
  device::BluetoothDevice* device = FindDevice(current_pairing_device_id_);
  if (!device) {
    BLUETOOTH_LOG(ERROR)
        << "OnRequestPinCode failed due to device no longer being "
           "found, identifier: "
        << current_pairing_device_id_;
    FinishCurrentPairingRequest(device::ConnectionFailureReason::kFailed);
    return;
  }

  BLUETOOTH_LOG(USER) << "Received pin code " << pin_code << " for device "
                      << current_pairing_device_id_;
  device->SetPinCode(pin_code);
}

void DevicePairingHandler::OnRequestPasskey(const std::string& passkey) {
  device::BluetoothDevice* device = FindDevice(current_pairing_device_id_);
  if (!device) {
    BLUETOOTH_LOG(ERROR)
        << "OnRequestPasskey failed due to device no longer being "
           "found, identifier: "
        << current_pairing_device_id_;
    FinishCurrentPairingRequest(device::ConnectionFailureReason::kFailed);
    return;
  }

  uint32_t passkey_num;
  if (base::StringToUint(passkey, &passkey_num)) {
    BLUETOOTH_LOG(USER) << "Received passkey " << passkey_num << " for device "
                        << current_pairing_device_id_;
    device->SetPasskey(passkey_num);
    return;
  }

  // If string to uint32_t conversion was unsuccessful, cancel the pairing.
  BLUETOOTH_LOG(ERROR) << "Converting " << passkey
                       << "to uint32_t failed, canceling pairing with "
                       << current_pairing_device_id_;
  CancelPairing();
}

void DevicePairingHandler::OnConfirmPairing(bool confirmed) {
  BLUETOOTH_LOG(EVENT) << "OnConfirmPairing() called with confirmed: "
                       << confirmed;

  device::BluetoothDevice* device = FindDevice(current_pairing_device_id_);
  if (!device) {
    BLUETOOTH_LOG(ERROR)
        << "OnConfirmPairing failed due to device no longer being "
           "found, identifier: "
        << current_pairing_device_id_;
    FinishCurrentPairingRequest(device::ConnectionFailureReason::kFailed);
    return;
  }

  if (confirmed)
    device->ConfirmPairing();
  else
    device->CancelPairing();
}

void DevicePairingHandler::FinishCurrentPairingRequest(
    absl::optional<device::ConnectionFailureReason> failure_reason) {
  // Reset state.
  is_canceling_pairing_ = false;
  device::BluetoothDevice* device = FindDevice(current_pairing_device_id_);
  current_pairing_device_id_.clear();

  device::BluetoothTransport transport =
      device ? device->GetType()
             : device::BluetoothTransport::BLUETOOTH_TRANSPORT_INVALID;

  device::RecordPairingResult(failure_reason, GetBluetoothTransport(transport),
                              base::Time::Now() - pairing_start_timestamp_);

  std::move(pair_device_callback_).Run(GetPairingResult(failure_reason));
}

void DevicePairingHandler::OnDelegateDisconnect() {
  BLUETOOTH_LOG(DEBUG) << "Delegate disconnected";

  // If the delegate disconnects and we have a pairing attempt, cancel the
  // pairing.
  if (!current_pairing_device_id_.empty()) {
    BLUETOOTH_LOG(EVENT) << "Delegate disconnected during pairing with "
                         << current_pairing_device_id_ << ", canceling pairing";
    CancelPairing();
  }

  delegate_.reset();
}

bool DevicePairingHandler::IsBluetoothEnabled() const {
  return adapter_state_controller_->GetAdapterState() ==
         mojom::BluetoothSystemState::kEnabled;
}

void DevicePairingHandler::FlushForTesting() {
  receiver_.FlushForTesting();  // IN-TEST
}

}  // namespace bluetooth_config
}  // namespace chromeos
