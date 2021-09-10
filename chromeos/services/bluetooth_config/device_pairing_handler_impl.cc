// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/device_pairing_handler_impl.h"

namespace chromeos {
namespace bluetooth_config {

DevicePairingHandlerImpl::DevicePairingHandlerImpl(
    mojo::PendingReceiver<mojom::DevicePairingHandler> pending_receiver,
    AdapterStateController* adapter_state_controller,
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
    base::OnceClosure finished_pairing_callback)
    : DevicePairingHandler(std::move(pending_receiver),
                           adapter_state_controller,
                           std::move(finished_pairing_callback)),
      bluetooth_adapter_(std::move(bluetooth_adapter)) {}

DevicePairingHandlerImpl::~DevicePairingHandlerImpl() {
  if (current_pairing_device_id().empty())
    return;

  // If we have a pairing attempt and this class is destroyed, cancel the
  // pairing.
  CancelPairing(mojom::PairingResult::kNonAuthFailure);
}

device::BluetoothDevice* DevicePairingHandlerImpl::FindDevice(
    const std::string& device_id) const {
  for (auto* device : bluetooth_adapter_->GetDevices()) {
    if (device->GetIdentifier() != device_id)
      continue;
    return device;
  }
  return nullptr;
}

}  // namespace bluetooth_config
}  // namespace chromeos
