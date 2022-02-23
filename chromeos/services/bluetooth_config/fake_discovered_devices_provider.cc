// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/fake_discovered_devices_provider.h"

namespace chromeos {
namespace bluetooth_config {

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

}  // namespace bluetooth_config
}  // namespace chromeos
