// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/fake_discovered_devices_provider.h"

namespace ash::bluetooth_config {

FakeDiscoveredDevicesProvider::FakeDiscoveredDevicesProvider() = default;

FakeDiscoveredDevicesProvider::~FakeDiscoveredDevicesProvider() = default;

void FakeDiscoveredDevicesProvider::SetDiscoveredDevices(
    std::vector<mojom::BluetoothDevicePropertiesPtr> discovered_devices) {
  discovered_devices_ = std::move(discovered_devices);
  NotifyDiscoveredDevicesListChanged();
}

std::vector<mojom::BluetoothDevicePropertiesPtr>
FakeDiscoveredDevicesProvider::GetDiscoveredDevices() const {
  std::vector<mojom::BluetoothDevicePropertiesPtr> discovered_devices;
  for (const auto& discovered_device : discovered_devices_)
    discovered_devices.push_back(discovered_device.Clone());
  return discovered_devices;
}

}  // namespace ash::bluetooth_config
