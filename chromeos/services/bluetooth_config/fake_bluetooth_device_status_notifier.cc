// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/fake_bluetooth_device_status_notifier.h"

namespace chromeos {
namespace bluetooth_config {

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

}  // namespace bluetooth_config
}  // namespace chromeos