// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/fake_device_pairing_handler.h"

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

void FakeDevicePairingHandler::SetDeviceList(
    std::vector<device::BluetoothDevice*> device_list) {
  device_list_ = std::move(device_list);
}

device::BluetoothDevice* FakeDevicePairingHandler::FindDevice(
    const std::string& device_id) const {
  for (auto* device : device_list_) {
    if (device->GetIdentifier() != device_id)
      continue;
    return device;
  }
  return nullptr;
}

}  // namespace bluetooth_config
}  // namespace chromeos
