// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_FAKE_DEVICE_PAIRING_HANDLER_H_
#define CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_FAKE_DEVICE_PAIRING_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/bluetooth_config/device_pairing_handler.h"

namespace ash::bluetooth_config {

class FakeDevicePairingHandler : public DevicePairingHandler {
 public:
  FakeDevicePairingHandler(
      mojo::PendingReceiver<mojom::DevicePairingHandler> pending_receiver,
      AdapterStateController* adapter_state_controller);
  ~FakeDevicePairingHandler() override;

  void SimulatePairDeviceFinished(
      std::optional<device::ConnectionFailureReason> failure_reason);

  void SimulateFetchDeviceFinished(mojom::BluetoothDevicePropertiesPtr device);

  // Methods to simulate the current pairing requiring authorization.
  void SimulateRequestPinCode();
  void SimulateRequestPasskey();
  void SimulateDisplayPinCode(const std::string& pin_code);
  void SimulateDisplayPasskey(uint32_t passkey);
  void SimulateKeysEntered(uint32_t entered);
  void SimulateConfirmPasskey(uint32_t passkey);
  void SimulateAuthorizePairing();

  const std::string& current_pairing_device_id() const {
    return DevicePairingHandler::current_pairing_device_id();
  }

  const std::optional<bool>& last_confirm() const { return last_confirm_; }

 private:
  // DevicePairingHandler:
  void FetchDevice(const std::string& device_address,
                   FetchDeviceCallback callback) override;
  void PerformPairDevice(const std::string& device_id) override;
  void PerformFinishCurrentPairingRequest(
      std::optional<device::ConnectionFailureReason> failure_reason,
      base::TimeDelta duration) override;
  void CancelPairing() override;
  void OnRequestPinCode(const std::string& pin_code) override;
  void OnRequestPasskey(const std::string& passkey) override;
  void OnConfirmPairing(bool confirmed) override;

  std::optional<bool> last_confirm_;

  std::vector<raw_ptr<device::BluetoothDevice, VectorExperimental>>
      device_list_;

  FetchDeviceCallback fetch_device_callback_;
};

}  // namespace ash::bluetooth_config

#endif  // CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_FAKE_DEVICE_PAIRING_HANDLER_H_
