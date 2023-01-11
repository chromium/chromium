// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/fake_bluetooth_discovery_delegate.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"

namespace ash::bluetooth_config {

FakeBluetoothDiscoveryDelegate::FakeBluetoothDiscoveryDelegate() = default;

FakeBluetoothDiscoveryDelegate::~FakeBluetoothDiscoveryDelegate() = default;

mojo::PendingRemote<mojom::BluetoothDiscoveryDelegate>
FakeBluetoothDiscoveryDelegate::GeneratePendingRemote() {
  auto pending_remote = receiver_.BindNewPipeAndPassRemote();
  receiver_.set_disconnect_handler(base::BindOnce(
      &FakeBluetoothDiscoveryDelegate::OnDisconnected, base::Unretained(this)));
  return pending_remote;
}

void FakeBluetoothDiscoveryDelegate::DisconnectMojoPipe() {
  receiver_.reset();

  // Allow the disconnection to propagate.
  base::RunLoop().RunUntilIdle();
}

bool FakeBluetoothDiscoveryDelegate::IsMojoPipeConnected() const {
  return receiver_.is_bound();
}

void FakeBluetoothDiscoveryDelegate::OnBluetoothDiscoveryStarted(
    mojo::PendingRemote<mojom::DevicePairingHandler> handler) {
  pairing_handler_.Bind(std::move(handler));
  ++num_start_callbacks_;
}

void FakeBluetoothDiscoveryDelegate::OnBluetoothDiscoveryStopped() {
  ++num_stop_callbacks_;
}

void FakeBluetoothDiscoveryDelegate::OnDiscoveredDevicesListChanged(
    std::vector<mojom::BluetoothDevicePropertiesPtr> discovered_devices) {
  discovered_devices_list_ = std::move(discovered_devices);
}

void FakeBluetoothDiscoveryDelegate::OnDisconnected() {
  receiver_.reset();
}

}  // namespace ash::bluetooth_config
