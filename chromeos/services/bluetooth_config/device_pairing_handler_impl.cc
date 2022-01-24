// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/device_pairing_handler_impl.h"

namespace chromeos {
namespace bluetooth_config {
namespace {
DevicePairingHandlerImpl::Factory* g_test_factory = nullptr;
}  // namespace

// static
std::unique_ptr<DevicePairingHandler> DevicePairingHandlerImpl::Factory::Create(
    mojo::PendingReceiver<mojom::DevicePairingHandler> pending_receiver,
    AdapterStateController* adapter_state_controller,
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
    base::OnceClosure finished_pairing_callback) {
  if (g_test_factory) {
    return g_test_factory->CreateInstance(
        std::move(pending_receiver), adapter_state_controller,
        bluetooth_adapter, std::move(finished_pairing_callback));
  }

  return base::WrapUnique(new DevicePairingHandlerImpl(
      std::move(pending_receiver), adapter_state_controller, bluetooth_adapter,
      std::move(finished_pairing_callback)));
}

// static
void DevicePairingHandlerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  g_test_factory = test_factory;
}

DevicePairingHandlerImpl::Factory::~Factory() = default;

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
  // If we have a pairing attempt and this class is destroyed, cancel the
  // pairing.
  if (!current_pairing_device_id().empty())
    CancelPairing();

  NotifyFinished();
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
