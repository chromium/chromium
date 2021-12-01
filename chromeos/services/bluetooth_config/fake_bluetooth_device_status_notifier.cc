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

void FakeBluetoothDeviceStatusNotifier::SetNewlyPairedDevices(
    const std::vector<mojom::PairedBluetoothDevicePropertiesPtr>& devices) {
  NotifyDevicesNewlyPaired(devices);
}

void FakeBluetoothDeviceStatusNotifier::SetConnectedDevices(
    const std::vector<mojom::PairedBluetoothDevicePropertiesPtr>& devices) {
  NotifyDevicesNewlyConnected(devices);
}

void FakeBluetoothDeviceStatusNotifier::SetDisconnectedDevices(
    const std::vector<mojom::PairedBluetoothDevicePropertiesPtr>& devices) {
  NotifyDevicesNewlyDisconnected(devices);
}

}  // namespace bluetooth_config
}  // namespace chromeos