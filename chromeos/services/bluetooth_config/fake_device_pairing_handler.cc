// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/fake_device_pairing_handler.h"

#include "chromeos/services/bluetooth_config/device_conversion_util.h"

namespace chromeos {
namespace bluetooth_config {

FakeDevicePairingHandler::FakeDevicePairingHandler(
    mojo::PendingReceiver<mojom::DevicePairingHandler> pending_receiver,
    AdapterStateController* adapter_state_controller,
    base::OnceClosure finished_pairing_callback)
    : DevicePairingHandler(std::move(pending_receiver),
                           adapter_state_controller,
                           std::move(finished_pairing_callback)) {}

FakeDevicePairingHandler::~FakeDevicePairingHandler() {
  // If we have a pairing attempt and this class is destroyed, cancel the
  // pairing.
  if (!current_pairing_device_id().empty())
    CancelPairing();

  NotifyFinished();
}

void FakeDevicePairingHandler::SimulatePairDeviceFinished(
    absl::optional<device::ConnectionFailureReason> failure_reason) {
  DCHECK(!current_pairing_device_id().empty());
  FinishCurrentPairingRequest(failure_reason);

  // If the pairing was a success, notify owner of this class pairing has
  // finished.
  if (!failure_reason.has_value())
    NotifyFinished();
}

void FakeDevicePairingHandler::SimulateFetchDeviceFinished(
    mojom::BluetoothDevicePropertiesPtr device) {
  std::move(fetch_device_callback_).Run(std::move(device));
}

void FakeDevicePairingHandler::SimulateRequestPinCode() {
  DCHECK(!current_pairing_device_id().empty());
  SendRequestPinCode();
}

void FakeDevicePairingHandler::SimulateRequestPasskey() {
  DCHECK(!current_pairing_device_id().empty());
  SendRequestPasskey();
}

void FakeDevicePairingHandler::SimulateDisplayPinCode(
    const std::string& pin_code) {
  DCHECK(!current_pairing_device_id().empty());
  SendDisplayPinCode(pin_code);
}

void FakeDevicePairingHandler::SimulateDisplayPasskey(uint32_t passkey) {
  DCHECK(!current_pairing_device_id().empty());
  SendDisplayPasskey(passkey);
}

void FakeDevicePairingHandler::SimulateKeysEntered(uint32_t entered) {
  DCHECK(!current_pairing_device_id().empty());
  SendKeysEntered(entered);
}

void FakeDevicePairingHandler::SimulateConfirmPasskey(uint32_t passkey) {
  DCHECK(!current_pairing_device_id().empty());
  SendConfirmPasskey(passkey);
}

void FakeDevicePairingHandler::SimulateAuthorizePairing() {
  DCHECK(!current_pairing_device_id().empty());
  SendAuthorizePairing();
}

void FakeDevicePairingHandler::FetchDevice(const std::string& device_address,
                                           FetchDeviceCallback callback) {
  fetch_device_callback_ = std::move(callback);
}

void FakeDevicePairingHandler::PerformPairDevice(const std::string& device_id) {
}

void FakeDevicePairingHandler::PerformFinishCurrentPairingRequest(
    absl::optional<device::ConnectionFailureReason> failure_reason,
    base::TimeDelta duration) {}

void FakeDevicePairingHandler::CancelPairing() {}

void FakeDevicePairingHandler::OnRequestPinCode(const std::string& pin_code) {}

void FakeDevicePairingHandler::OnRequestPasskey(const std::string& passkey) {}

void FakeDevicePairingHandler::OnConfirmPairing(bool confirmed) {}

}  // namespace bluetooth_config
}  // namespace chromeos
