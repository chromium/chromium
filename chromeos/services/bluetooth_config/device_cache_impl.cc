// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/device_cache_impl.h"

#include <algorithm>

#include "chromeos/services/bluetooth_config/device_conversion_util.h"

namespace chromeos {
namespace bluetooth_config {
namespace {

mojom::PairedBluetoothDevicePropertiesPtr
GeneratePairedBluetoothDeviceProperties(const device::BluetoothDevice* device) {
  mojom::PairedBluetoothDevicePropertiesPtr properties =
      mojom::PairedBluetoothDeviceProperties::New();
  properties->device_properties = GenerateBluetoothDeviceMojoProperties(device);
  // TODO(khorimoto): Add paired device nickname property.
  return properties;
}

}  // namespace

DeviceCacheImpl::DeviceCacheImpl(
    AdapterStateController* adapter_state_controller_param,
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter)
    : DeviceCache(adapter_state_controller_param),
      bluetooth_adapter_(std::move(bluetooth_adapter)) {
  adapter_state_controller_observation_.Observe(adapter_state_controller());
  adapter_observation_.Observe(bluetooth_adapter_.get());

  FetchInitialPairedDeviceList();
}

DeviceCacheImpl::~DeviceCacheImpl() = default;

std::vector<mojom::PairedBluetoothDevicePropertiesPtr>
DeviceCacheImpl::PerformGetPairedDevices() const {
  std::vector<mojom::PairedBluetoothDevicePropertiesPtr> paired_devices;
  for (const auto& paired_device : paired_devices_)
    paired_devices.push_back(paired_device.Clone());
  return paired_devices;
}

void DeviceCacheImpl::OnAdapterStateChanged() {
  NotifyPairedDevicesListChanged();
}

void DeviceCacheImpl::DeviceAdded(device::BluetoothAdapter* adapter,
                                  device::BluetoothDevice* device) {
  AttemptSetDeviceInPairedDeviceList(device);
}

void DeviceCacheImpl::DeviceRemoved(device::BluetoothAdapter* adapter,
                                    device::BluetoothDevice* device) {
  RemoveFromPairedDeviceList(device);
}

void DeviceCacheImpl::DeviceChanged(device::BluetoothAdapter* adapter,
                                    device::BluetoothDevice* device) {
  AttemptUpdatePairedDeviceMetadata(device);
}

void DeviceCacheImpl::DevicePairedChanged(device::BluetoothAdapter* adapter,
                                          device::BluetoothDevice* device,
                                          bool new_paired_status) {
  if (new_paired_status)
    AttemptUpdatePairedDeviceMetadata(device);
  else
    RemoveFromPairedDeviceList(device);
}

void DeviceCacheImpl::DeviceConnectedStateChanged(
    device::BluetoothAdapter* adapter,
    device::BluetoothDevice* device,
    bool is_now_connected) {
  AttemptUpdatePairedDeviceMetadata(device);
}

void DeviceCacheImpl::DeviceBatteryChanged(
    device::BluetoothAdapter* adapter,
    device::BluetoothDevice* device,
    absl::optional<uint8_t> new_battery_percentage) {
  AttemptUpdatePairedDeviceMetadata(device);
}

void DeviceCacheImpl::FetchInitialPairedDeviceList() {
  for (const device::BluetoothDevice* device :
       bluetooth_adapter_->GetDevices()) {
    paired_devices_.push_back(GeneratePairedBluetoothDeviceProperties(device));
  }

  SortPairedDeviceList();
}

void DeviceCacheImpl::AttemptSetDeviceInPairedDeviceList(
    device::BluetoothDevice* device) {
  if (!device->IsPaired())
    return;

  // Remove the old (stale) properties, if they exist.
  RemoveFromPairedDeviceList(device);

  paired_devices_.push_back(GeneratePairedBluetoothDeviceProperties(device));
  SortPairedDeviceList();
  NotifyPairedDevicesListChanged();
}

void DeviceCacheImpl::RemoveFromPairedDeviceList(
    device::BluetoothDevice* device) {
  auto it = paired_devices_.begin();
  while (it != paired_devices_.end()) {
    if (device->GetIdentifier() == (*it)->device_properties->id) {
      paired_devices_.erase(it);
      NotifyPairedDevicesListChanged();
      return;
    }

    ++it;
  }
}

void DeviceCacheImpl::AttemptUpdatePairedDeviceMetadata(
    device::BluetoothDevice* device) {
  // Remove existing metadata about |device|.
  RemoveFromPairedDeviceList(device);

  // Now, add updated metadata.
  AttemptSetDeviceInPairedDeviceList(device);
}

void DeviceCacheImpl::SortPairedDeviceList() {
  std::sort(paired_devices_.begin(), paired_devices_.end(),
            [](const mojom::PairedBluetoothDevicePropertiesPtr& first,
               const mojom::PairedBluetoothDevicePropertiesPtr& second) {
              return first->device_properties->connection_state >
                     second->device_properties->connection_state;
            });
}

}  // namespace bluetooth_config
}  // namespace chromeos
