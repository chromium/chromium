// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/device_pairing_handler.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/device_event_log/device_event_log.h"

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

}  // namespace

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
  device::BluetoothDevice* device = FindDevice(current_pairing_device_id_);
  if (!device) {
    BLUETOOTH_LOG(ERROR)
        << "Could not cancel pairing for device to due device no longer being "
           "found, identifier: "
        << current_pairing_device_id_;
    FinishCurrentPairingRequest(mojom::PairingResult::kAuthFailed);
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
  // There should only be one PairDevice request at a time.
  CHECK(current_pairing_device_id_.empty());

  pair_device_callback_ = std::move(callback);

  delegate_.reset();
  delegate_.Bind(std::move(delegate));
  delegate_.set_disconnect_handler(base::BindOnce(
      &DevicePairingHandler::OnDelegateDisconnect, base::Unretained(this)));

  // If Bluetooth is not enabled, fail immediately.
  if (!IsBluetoothEnabled()) {
    BLUETOOTH_LOG(ERROR) << "Pairing failed due to Bluetooth not being "
                            "enabled, device identifier: "
                         << device_id;
    FinishCurrentPairingRequest(mojom::PairingResult::kNonAuthFailure);
    return;
  }

  // Find the device and attempt to pair to it.
  device::BluetoothDevice* device = FindDevice(device_id);

  if (!device) {
    BLUETOOTH_LOG(ERROR) << "Pairing failed due to device not being "
                            "found, identifier: "
                         << device_id;
    FinishCurrentPairingRequest(mojom::PairingResult::kNonAuthFailure);
    return;
  }

  current_pairing_device_id_ = device_id;
  device->Connect(
      /*delegate=*/this, base::BindOnce(&DevicePairingHandler::OnDeviceConnect,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void DevicePairingHandler::RequestPinCode(device::BluetoothDevice* device) {
  DCHECK(device->GetIdentifier() == current_pairing_device_id_);
  delegate_->RequestPinCode(base::BindOnce(
      &DevicePairingHandler::OnRequestPinCode, weak_ptr_factory_.GetWeakPtr()));
}

void DevicePairingHandler::RequestPasskey(device::BluetoothDevice* device) {
  DCHECK(device->GetIdentifier() == current_pairing_device_id_);
  delegate_->RequestPasskey(base::BindOnce(
      &DevicePairingHandler::OnRequestPasskey, weak_ptr_factory_.GetWeakPtr()));
}

void DevicePairingHandler::DisplayPinCode(device::BluetoothDevice* device,
                                          const std::string& pin_code) {
  DCHECK(device->GetIdentifier() == current_pairing_device_id_);
  key_entered_handler_.reset();
  delegate_->DisplayPinCode(pin_code,
                            key_entered_handler_.BindNewPipeAndPassReceiver());
}

void DevicePairingHandler::DisplayPasskey(device::BluetoothDevice* device,
                                          uint32_t passkey) {
  DCHECK(device->GetIdentifier() == current_pairing_device_id_);
  key_entered_handler_.reset();
  delegate_->DisplayPasskey(PasskeyToString(passkey),
                            key_entered_handler_.BindNewPipeAndPassReceiver());
}

void DevicePairingHandler::KeysEntered(device::BluetoothDevice* device,
                                       uint32_t entered) {
  DCHECK(device->GetIdentifier() == current_pairing_device_id_);
  key_entered_handler_->HandleKeyEntered(entered);
}

void DevicePairingHandler::ConfirmPasskey(device::BluetoothDevice* device,
                                          uint32_t passkey) {
  DCHECK(device->GetIdentifier() == current_pairing_device_id_);
  delegate_->ConfirmPasskey(
      PasskeyToString(passkey),
      base::BindOnce(&DevicePairingHandler::OnConfirmPairing,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DevicePairingHandler::AuthorizePairing(device::BluetoothDevice* device) {
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
  CancelPairing();
}

void DevicePairingHandler::OnDeviceConnect(
    absl::optional<device::BluetoothDevice::ConnectErrorCode> error_code) {
  if (!error_code.has_value()) {
    FinishCurrentPairingRequest(mojom::PairingResult::kSuccess);
    NotifyFinished();
    return;
  }

  BLUETOOTH_LOG(ERROR) << "Pairing failed with error code: "
                       << error_code.value();
  using ErrorCode = device::BluetoothDevice::ConnectErrorCode;
  switch (error_code.value()) {
    case ErrorCode::ERROR_AUTH_CANCELED:
      FALLTHROUGH;
    case ErrorCode::ERROR_AUTH_FAILED:
      FALLTHROUGH;
    case ErrorCode::ERROR_AUTH_REJECTED:
      FALLTHROUGH;
    case ErrorCode::ERROR_AUTH_TIMEOUT:
      FinishCurrentPairingRequest(mojom::PairingResult::kAuthFailed);
      return;

    case ErrorCode::ERROR_FAILED:
      FALLTHROUGH;
    case ErrorCode::ERROR_INPROGRESS:
      FALLTHROUGH;
    case ErrorCode::ERROR_UNKNOWN:
      FALLTHROUGH;
    case ErrorCode::ERROR_UNSUPPORTED_DEVICE:
      FinishCurrentPairingRequest(mojom::PairingResult::kNonAuthFailure);
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
    FinishCurrentPairingRequest(mojom::PairingResult::kNonAuthFailure);
    return;
  }

  device->SetPinCode(pin_code);
}

void DevicePairingHandler::OnRequestPasskey(const std::string& passkey) {
  device::BluetoothDevice* device = FindDevice(current_pairing_device_id_);
  if (!device) {
    BLUETOOTH_LOG(ERROR)
        << "OnRequestPasskey failed due to device no longer being "
           "found, identifier: "
        << current_pairing_device_id_;
    FinishCurrentPairingRequest(mojom::PairingResult::kNonAuthFailure);
    return;
  }

  uint32_t passkey_num;
  if (base::StringToUint(passkey, &passkey_num)) {
    device->SetPasskey(passkey_num);
    return;
  }

  // If string to uint32_t conversion was unsuccessful, cancel the pairing.
  CancelPairing();
}

void DevicePairingHandler::OnConfirmPairing(bool confirmed) {
  device::BluetoothDevice* device = FindDevice(current_pairing_device_id_);
  if (!device) {
    BLUETOOTH_LOG(ERROR)
        << "OnConfirmPairing failed due to device no longer being "
           "found, identifier: "
        << current_pairing_device_id_;
    FinishCurrentPairingRequest(mojom::PairingResult::kNonAuthFailure);
    return;
  }

  if (confirmed)
    device->ConfirmPairing();
  else
    device->CancelPairing();
}

void DevicePairingHandler::FinishCurrentPairingRequest(
    mojom::PairingResult result) {
  current_pairing_device_id_.clear();
  std::move(pair_device_callback_).Run(result);
}

void DevicePairingHandler::OnDelegateDisconnect() {
  // If the delegate disconnects and we have a pairing attempt, cancel the
  // pairing.
  if (!current_pairing_device_id_.empty())
    CancelPairing();

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
