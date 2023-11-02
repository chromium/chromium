// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/fake_device_cache.h"

namespace ash::bluetooth_config {

FakeDeviceCache::FakeDeviceCache(
    AdapterStateController* adapter_state_controller)
    : DeviceCache(adapter_state_controller) {}

FakeDeviceCache::~FakeDeviceCache() = default;

void FakeDeviceCache::SetPairedDevices(
    std::vector<mojom::PairedBluetoothDevicePropertiesPtr> paired_devices) {
  paired_devices_ = std::move(paired_devices);
  NotifyPairedDevicesListChanged();
}

void FakeDeviceCache::SetUnpairedDevices(
    std::vector<mojom::BluetoothDevicePropertiesPtr> unpaired_devices) {
  unpaired_devices_ = std::move(unpaired_devices);
  NotifyUnpairedDevicesListChanged();
}

std::vector<mojom::PairedBluetoothDevicePropertiesPtr>
FakeDeviceCache::PerformGetPairedDevices() const {
  std::vector<mojom::PairedBluetoothDevicePropertiesPtr> paired_devices;
  for (const auto& paired_device : paired_devices_)
    paired_devices.push_back(paired_device.Clone());
  return paired_devices;
}

std::vector<mojom::BluetoothDevicePropertiesPtr>
FakeDeviceCache::PerformGetUnpairedDevices() const {
  std::vector<mojom::BluetoothDevicePropertiesPtr> unpaired_devices;
  for (const auto& unpaired_device : unpaired_devices_)
    unpaired_devices.push_back(unpaired_device.Clone());
  return unpaired_devices;
}

}  // namespace ash::bluetooth_config
