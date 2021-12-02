// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/bluetooth_device_status_notifier_impl.h"
#include "chromeos/services/bluetooth_config/device_cache.h"

#include <vector>

namespace chromeos {
namespace bluetooth_config {

BluetoothDeviceStatusNotifierImpl::BluetoothDeviceStatusNotifierImpl(
    DeviceCache* device_cache)
    : device_cache_(device_cache) {
  device_cache_observation_.Observe(device_cache_);

  for (const auto& paired_device : device_cache_->GetPairedDevices()) {
    devices_id_to_properties_map_[paired_device->device_properties->id] =
        paired_device.Clone();
  }
}

BluetoothDeviceStatusNotifierImpl::~BluetoothDeviceStatusNotifierImpl() =
    default;

void BluetoothDeviceStatusNotifierImpl::OnPairedDevicesListChanged() {
  CheckForDeviceStateChange();
}

void BluetoothDeviceStatusNotifierImpl::CheckForDeviceStateChange() {
  const std::vector<mojom::PairedBluetoothDevicePropertiesPtr> paired_devices =
      device_cache_->GetPairedDevices();

  if (paired_devices.empty()) {
    devices_id_to_properties_map_.clear();
    return;
  }

  // Store old map in a temporary map, this is done so if a device is unpaired
  // |devices_id_to_properties_map_| will always contain only currently paired
  // devices.
  std::unordered_map<std::string, mojom::PairedBluetoothDevicePropertiesPtr>
      previous_devices_id_to_properties_map =
          std::move(devices_id_to_properties_map_);
  devices_id_to_properties_map_.clear();

  for (const auto& device : paired_devices) {
    devices_id_to_properties_map_[device->device_properties->id] =
        device.Clone();

    auto it = previous_devices_id_to_properties_map.find(
        device->device_properties->id);

    // Check if device is not in previous map and is connected. If it is not,
    // this means a new paired device was found.
    if (it == previous_devices_id_to_properties_map.end()) {
      if (device->device_properties->connection_state ==
          mojom::DeviceConnectionState::kConnected) {
        NotifyDeviceNewlyPaired(device);
      }
      continue;
    }

    // Check if device is recently disconnected.
    if (it->second->device_properties->connection_state ==
            mojom::DeviceConnectionState::kConnected &&
        device->device_properties->connection_state ==
            mojom::DeviceConnectionState::kNotConnected) {
      NotifyDeviceNewlyDisconnected(device);
      continue;
    }

    // Check if device is recently connected.
    if (it->second->device_properties->connection_state !=
            mojom::DeviceConnectionState::kConnected &&
        device->device_properties->connection_state ==
            mojom::DeviceConnectionState::kConnected) {
      NotifyDeviceNewlyConnected(device);
      continue;
    }
  }
}

}  // namespace bluetooth_config
}  // namespace chromeos
