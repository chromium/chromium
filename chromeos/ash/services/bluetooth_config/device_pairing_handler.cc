// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/device_pairing_handler.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/default_clock.h"
#include "components/device_event_log/device_event_log.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/chromeos/bluetooth_utils.h"

namespace ash::bluetooth_config {

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

mojom::PairingResult GetPairingResult(
    std::optional<device::ConnectionFailureReason> failure_reason) {
  if (!failure_reason) {
    return mojom::PairingResult::kSuccess;
  }

  switch (failure_reason.value()) {
    case device::ConnectionFailureReason::kAuthTimeout:
      [[fallthrough]];
    case device::ConnectionFailureReason::kAuthFailed:
      [[fallthrough]];
    case device::ConnectionFailureReason::kAuthCanceled:
      [[fallthrough]];
    case device::ConnectionFailureReason::kAuthRejected:
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
      [[fallthrough]];
    case device::ConnectionFailureReason::kInprogress:
      [[fallthrough]];
    case device::ConnectionFailureReason::kNotFound:
      [[fallthrough]];
    case device::ConnectionFailureReason::kBluetoothDisabled:
      [[fallthrough]];
    case device::ConnectionFailureReason::kDeviceNotReady:
      [[fallthrough]];
    case device::ConnectionFailureReason::kAlreadyConnected:
      [[fallthrough]];
    case device::ConnectionFailureReason::kDeviceAlreadyExists:
      [[fallthrough]];
    case device::ConnectionFailureReason::kInvalidArgs:
      [[fallthrough]];
    case device::ConnectionFailureReason::kNonAuthTimeout:
      [[fallthrough]];
    case device::ConnectionFailureReason::kNoMemory:
      [[fallthrough]];
    case device::ConnectionFailureReason::kJniEnvironment:
      [[fallthrough]];
    case device::ConnectionFailureReason::kJniThreadAttach:
      [[fallthrough]];
    case device::ConnectionFailureReason::kWakelock:
      [[fallthrough]];
    case device::ConnectionFailureReason::kUnexpectedState:
      [[fallthrough]];
    case device::ConnectionFailureReason::kSocketError:
      return mojom::PairingResult::kNonAuthFailure;
  }
}

}  // namespace

DevicePairingHandler::DevicePairingHandler(
    mojo::PendingReceiver<mojom::DevicePairingHandler> pending_receiver,
    AdapterStateController* adapter_state_controller)
    : adapter_state_controller_(adapter_state_controller) {
  adapter_state_controller_observation_.Observe(
      adapter_state_controller_.get());
  receiver_.Bind(std::move(pending_receiver));
}

DevicePairingHandler::~DevicePairingHandler() = default;

void DevicePairingHandler::SendRequestPinCode() {
  BLUETOOTH_LOG(EVENT) << "Requesting pin code for "
                       << current_pairing_device_id_;
  delegate_->RequestPinCode(base::BindOnce(
      &DevicePairingHandler::OnRequestPinCode, weak_ptr_factory_.GetWeakPtr()));
}
void DevicePairingHandler::SendRequestPasskey() {
  BLUETOOTH_LOG(EVENT) << "Requesting passkey for "
                       << current_pairing_device_id_;
  delegate_->RequestPasskey(base::BindOnce(
      &DevicePairingHandler::OnRequestPasskey, weak_ptr_factory_.GetWeakPtr()));
}

void DevicePairingHandler::SendDisplayPinCode(const std::string& pin_code) {
  BLUETOOTH_LOG(EVENT) << "Displaying pin code for "
                       << current_pairing_device_id_
                       << ", pin code: " << pin_code;
  key_entered_handler_.reset();
  delegate_->DisplayPinCode(pin_code,
                            key_entered_handler_.BindNewPipeAndPassReceiver());
}

void DevicePairingHandler::SendDisplayPasskey(uint32_t passkey) {
  BLUETOOTH_LOG(EVENT) << "Displaying passkey for "
                       << current_pairing_device_id_
                       << ", passkey: " << passkey;
  key_entered_handler_.reset();
  delegate_->DisplayPasskey(PasskeyToString(passkey),
                            key_entered_handler_.BindNewPipeAndPassReceiver());
}

void DevicePairingHandler::SendKeysEntered(uint32_t entered) {
  BLUETOOTH_LOG(EVENT) << entered << " keys entered for "
                       << current_pairing_device_id_;
  key_entered_handler_->HandleKeyEntered(entered);
}

void DevicePairingHandler::SendConfirmPasskey(uint32_t passkey) {
  BLUETOOTH_LOG(EVENT) << "Confirming passkey for "
                       << current_pairing_device_id_
                       << ", passkey: " << passkey;
  delegate_->ConfirmPasskey(
      PasskeyToString(passkey),
      base::BindOnce(&DevicePairingHandler::OnConfirmPairing,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DevicePairingHandler::SendAuthorizePairing() {
  BLUETOOTH_LOG(EVENT) << "Authorizing pairing for "
                       << current_pairing_device_id();
  delegate_->AuthorizePairing(base::BindOnce(
      &DevicePairingHandler::OnConfirmPairing, weak_ptr_factory_.GetWeakPtr()));
}

void DevicePairingHandler::FinishCurrentPairingRequest(
    std::optional<device::ConnectionFailureReason> failure_reason) {
  PerformFinishCurrentPairingRequest(
      failure_reason, base::Time::Now() - pairing_start_timestamp_);
  current_pairing_device_id_.clear();

  // |pair_device_callback_| can be null if |receiver_| has already been
  // disconnected before this method is invoked.
  if (!pair_device_callback_) {
    BLUETOOTH_LOG(EVENT)
        << "FinishCurrentPairingRequest() called with |pair_device_callback_| "
           "null, not running callback";
    return;
  }

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

void DevicePairingHandler::PairDevice(
    const std::string& device_id,
    mojo::PendingRemote<mojom::DevicePairingDelegate> delegate,
    PairDeviceCallback callback) {
  BLUETOOTH_LOG(USER) << "Attempting to pair with device " << device_id;

  // In certain situations, such as when the pairing dialog is initiated from
  // both the system tray and ChromeOS settings, an active pairing request
  // might be underway when the pairDevice function is called. In such
  // instances, cancel the second pairing request. (See b/311813249)
  if (!current_pairing_device_id_.empty()) {
    std::move(callback).Run(mojom::PairingResult::kNonAuthFailure);
    return;
  }

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
    FinishCurrentPairingRequest(
        device::ConnectionFailureReason::kBluetoothDisabled);
    return;
  }

  current_pairing_device_id_ = device_id;
  PerformPairDevice(current_pairing_device_id_);
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

bool DevicePairingHandler::IsBluetoothEnabled() const {
  return adapter_state_controller_->GetAdapterState() ==
         mojom::BluetoothSystemState::kEnabled;
}

void DevicePairingHandler::FlushForTesting() {
  receiver_.FlushForTesting();  // IN-TEST
}

}  // namespace ash::bluetooth_config
