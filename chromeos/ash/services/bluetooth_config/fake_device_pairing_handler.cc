// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/fake_device_pairing_handler.h"

#include "base/run_loop.h"
#include "chromeos/ash/services/bluetooth_config/device_conversion_util.h"

namespace ash::bluetooth_config {

FakeDevicePairingHandler::FakeDevicePairingHandler(
    mojo::PendingReceiver<mojom::DevicePairingHandler> pending_receiver,
    AdapterStateController* adapter_state_controller)
    : DevicePairingHandler(std::move(pending_receiver),
                           adapter_state_controller) {}

FakeDevicePairingHandler::~FakeDevicePairingHandler() {
  if (current_pairing_device_id().empty())
    return;

  // If we have a pairing attempt and this class is destroyed, cancel the
  // pairing.
  CancelPairing();
}

void FakeDevicePairingHandler::SimulatePairDeviceFinished(
    std::optional<device::ConnectionFailureReason> failure_reason) {
  DCHECK(!current_pairing_device_id().empty());
  FinishCurrentPairingRequest(failure_reason);
  base::RunLoop().RunUntilIdle();
}

void FakeDevicePairingHandler::SimulateFetchDeviceFinished(
    mojom::BluetoothDevicePropertiesPtr device) {
  std::move(fetch_device_callback_).Run(std::move(device));
}

void FakeDevicePairingHandler::SimulateRequestPinCode() {
  DCHECK(!current_pairing_device_id().empty());
  SendRequestPinCode();
  base::RunLoop().RunUntilIdle();
}

void FakeDevicePairingHandler::SimulateRequestPasskey() {
  DCHECK(!current_pairing_device_id().empty());
  SendRequestPasskey();
  base::RunLoop().RunUntilIdle();
}

void FakeDevicePairingHandler::SimulateDisplayPinCode(
    const std::string& pin_code) {
  DCHECK(!current_pairing_device_id().empty());
  SendDisplayPinCode(pin_code);
  base::RunLoop().RunUntilIdle();
}

void FakeDevicePairingHandler::SimulateDisplayPasskey(uint32_t passkey) {
  DCHECK(!current_pairing_device_id().empty());
  SendDisplayPasskey(passkey);
  base::RunLoop().RunUntilIdle();
}

void FakeDevicePairingHandler::SimulateKeysEntered(uint32_t entered) {
  DCHECK(!current_pairing_device_id().empty());
  SendKeysEntered(entered);
  base::RunLoop().RunUntilIdle();
}

void FakeDevicePairingHandler::SimulateConfirmPasskey(uint32_t passkey) {
  DCHECK(!current_pairing_device_id().empty());
  SendConfirmPasskey(passkey);
  base::RunLoop().RunUntilIdle();
}

void FakeDevicePairingHandler::SimulateAuthorizePairing() {
  DCHECK(!current_pairing_device_id().empty());
  SendAuthorizePairing();
  base::RunLoop().RunUntilIdle();
}

void FakeDevicePairingHandler::FetchDevice(const std::string& device_address,
                                           FetchDeviceCallback callback) {
  fetch_device_callback_ = std::move(callback);
}

void FakeDevicePairingHandler::PerformPairDevice(const std::string& device_id) {
}

void FakeDevicePairingHandler::PerformFinishCurrentPairingRequest(
    std::optional<device::ConnectionFailureReason> failure_reason,
    base::TimeDelta duration) {}

void FakeDevicePairingHandler::CancelPairing() {
  base::RunLoop().RunUntilIdle();
  FinishCurrentPairingRequest(device::ConnectionFailureReason::kFailed);
  base::RunLoop().RunUntilIdle();
}

void FakeDevicePairingHandler::OnRequestPinCode(const std::string& pin_code) {}

void FakeDevicePairingHandler::OnRequestPasskey(const std::string& passkey) {}

void FakeDevicePairingHandler::OnConfirmPairing(bool confirmed) {
  last_confirm_ = confirmed;
  if (confirmed) {
    FinishCurrentPairingRequest(/*failure_reason=*/std::nullopt);
  } else {
    FinishCurrentPairingRequest(device::ConnectionFailureReason::kAuthFailed);
  }
  base::RunLoop().RunUntilIdle();
}

}  // namespace ash::bluetooth_config
