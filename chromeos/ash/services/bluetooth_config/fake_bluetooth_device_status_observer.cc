// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/fake_bluetooth_device_status_observer.h"

#include <utility>

#include "base/run_loop.h"

namespace ash::bluetooth_config {

FakeBluetoothDeviceStatusObserver::FakeBluetoothDeviceStatusObserver() =
    default;

FakeBluetoothDeviceStatusObserver::~FakeBluetoothDeviceStatusObserver() =
    default;

mojo::PendingRemote<mojom::BluetoothDeviceStatusObserver>
FakeBluetoothDeviceStatusObserver::GeneratePendingRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

void FakeBluetoothDeviceStatusObserver::DisconnectMojoPipe() {
  receiver_.reset();

  // Allow the disconnection to propagate.
  base::RunLoop().RunUntilIdle();
}

void FakeBluetoothDeviceStatusObserver::OnDevicePaired(
    mojom::PairedBluetoothDevicePropertiesPtr device) {
  paired_device_properties_list_.push_back(std::move(device));
}

void FakeBluetoothDeviceStatusObserver::OnDeviceConnected(
    mojom::PairedBluetoothDevicePropertiesPtr device) {
  connected_device_properties_list_.push_back(std::move(device));
}

void FakeBluetoothDeviceStatusObserver::OnDeviceDisconnected(
    mojom::PairedBluetoothDevicePropertiesPtr device) {
  disconnected_device_properties_list_.push_back(std::move(device));
}

}  // namespace ash::bluetooth_config
