// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_BLUETOOTH_CONFIG_FAKE_DEVICE_PAIRING_HANDLER_H_
#define CHROMEOS_SERVICES_BLUETOOTH_CONFIG_FAKE_DEVICE_PAIRING_HANDLER_H_

#include "chromeos/services/bluetooth_config/device_pairing_handler.h"

namespace chromeos {
namespace bluetooth_config {

class FakeDevicePairingHandler : public DevicePairingHandler {
 public:
  FakeDevicePairingHandler(
      mojo::PendingReceiver<mojom::DevicePairingHandler> pending_receiver,
      AdapterStateController* adapter_state_controller,
      base::OnceClosure finished_pairing_callback);
  ~FakeDevicePairingHandler() override;

  void SetDeviceList(std::vector<device::BluetoothDevice*> device_list);

 private:
  // DevicePairingHandler:
  device::BluetoothDevice* FindDevice(
      const std::string& device_id) const override;

  std::vector<device::BluetoothDevice*> device_list_;
};

}  // namespace bluetooth_config
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_BLUETOOTH_CONFIG_FAKE_DEVICE_PAIRING_HANDLER_H_
