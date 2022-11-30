// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/fake_bluetooth_device_status_notifier.h"

namespace ash::bluetooth_config {

FakeBluetoothDeviceStatusNotifier::FakeBluetoothDeviceStatusNotifier() =
    default;

FakeBluetoothDeviceStatusNotifier::~FakeBluetoothDeviceStatusNotifier() =
    default;

void FakeBluetoothDeviceStatusNotifier::SetNewlyPairedDevice(
    const mojom::PairedBluetoothDevicePropertiesPtr& device) {
  NotifyDeviceNewlyPaired(device);
}

void FakeBluetoothDeviceStatusNotifier::SetConnectedDevice(
    const mojom::PairedBluetoothDevicePropertiesPtr& device) {
  NotifyDeviceNewlyConnected(device);
}

void FakeBluetoothDeviceStatusNotifier::SetDisconnectedDevice(
    const mojom::PairedBluetoothDevicePropertiesPtr& device) {
  NotifyDeviceNewlyDisconnected(device);
}

}  // namespace ash::bluetooth_config
