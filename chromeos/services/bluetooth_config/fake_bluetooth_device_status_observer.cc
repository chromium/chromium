// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/fake_bluetooth_device_status_observer.h"

#include <utility>

#include "base/run_loop.h"

namespace chromeos {
namespace bluetooth_config {

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

}  // namespace bluetooth_config
}  // namespace chromeos