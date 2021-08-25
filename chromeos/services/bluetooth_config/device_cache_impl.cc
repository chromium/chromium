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

DeviceCacheImpl::UnpairedDevice::UnpairedDevice(
    const device::BluetoothDevice* device)
    : device_properties(GenerateBluetoothDeviceMojoProperties(device)),
      inquiry_rssi(device->GetInquiryRSSI()) {}

DeviceCacheImpl::UnpairedDevice::~UnpairedDevice() = default;

DeviceCacheImpl::DeviceCacheImpl(
    AdapterStateController* adapter_state_controller_param,
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter)
    : DeviceCache(adapter_state_controller_param),
      bluetooth_adapter_(std::move(bluetooth_adapter)) {
  adapter_state_controller_observation_.Observe(adapter_state_controller());
  adapter_observation_.Observe(bluetooth_adapter_.get());

  FetchInitialDeviceLists();
}

DeviceCacheImpl::~DeviceCacheImpl() = default;

std::vector<mojom::PairedBluetoothDevicePropertiesPtr>
DeviceCacheImpl::PerformGetPairedDevices() const {
  std::vector<mojom::PairedBluetoothDevicePropertiesPtr> paired_devices;
  for (const auto& paired_device : paired_devices_)
    paired_devices.push_back(paired_device.Clone());
  return paired_devices;
}

std::vector<mojom::BluetoothDevicePropertiesPtr>
DeviceCacheImpl::PerformGetUnpairedDevices() const {
  std::vector<mojom::BluetoothDevicePropertiesPtr> unpaired_devices;
  for (const auto& unpaired_device : unpaired_devices_)
    unpaired_devices.push_back(unpaired_device->device_properties.Clone());
  return unpaired_devices;
}

void DeviceCacheImpl::OnAdapterStateChanged() {
  NotifyPairedDevicesListChanged();
  NotifyUnpairedDevicesListChanged();
}

void DeviceCacheImpl::DeviceAdded(device::BluetoothAdapter* adapter,
                                  device::BluetoothDevice* device) {
  if (device->IsPaired())
    AttemptSetDeviceInPairedDeviceList(device);
  else
    AttemptSetDeviceInUnpairedDeviceList(device);
}

void DeviceCacheImpl::DeviceRemoved(device::BluetoothAdapter* adapter,
                                    device::BluetoothDevice* device) {
  if (device->IsPaired())
    RemoveFromPairedDeviceList(device);
  else
    RemoveFromUnpairedDeviceList(device);
}

void DeviceCacheImpl::DeviceChanged(device::BluetoothAdapter* adapter,
                                    device::BluetoothDevice* device) {
  if (device->IsPaired())
    AttemptUpdatePairedDeviceMetadata(device);
  else
    AttemptUpdateUnpairedDeviceMetadata(device);
}

void DeviceCacheImpl::DevicePairedChanged(device::BluetoothAdapter* adapter,
                                          device::BluetoothDevice* device,
                                          bool new_paired_status) {
  if (new_paired_status) {
    RemoveFromUnpairedDeviceList(device);
    AttemptUpdatePairedDeviceMetadata(device);
    return;
  }
  RemoveFromPairedDeviceList(device);
  AttemptUpdateUnpairedDeviceMetadata(device);
}

void DeviceCacheImpl::DeviceConnectedStateChanged(
    device::BluetoothAdapter* adapter,
    device::BluetoothDevice* device,
    bool is_now_connected) {
  DCHECK(device->IsPaired());
  AttemptUpdatePairedDeviceMetadata(device);
}

void DeviceCacheImpl::DeviceBatteryChanged(
    device::BluetoothAdapter* adapter,
    device::BluetoothDevice* device,
    absl::optional<uint8_t> new_battery_percentage) {
  DeviceChanged(adapter, device);
}

void DeviceCacheImpl::FetchInitialDeviceLists() {
  for (const device::BluetoothDevice* device :
       bluetooth_adapter_->GetDevices()) {
    if (device->IsPaired()) {
      paired_devices_.push_back(
          GeneratePairedBluetoothDeviceProperties(device));
    } else {
      unpaired_devices_.push_back(std::make_unique<UnpairedDevice>(device));
    }
  }

  SortPairedDeviceList();
  SortUnpairedDeviceList();
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

void DeviceCacheImpl::AttemptSetDeviceInUnpairedDeviceList(
    device::BluetoothDevice* device) {
  if (device->IsPaired())
    return;

  // Remove the old (stale) properties, if they exist.
  RemoveFromUnpairedDeviceList(device);

  unpaired_devices_.push_back(std::make_unique<UnpairedDevice>(device));
  SortUnpairedDeviceList();
  NotifyUnpairedDevicesListChanged();
}

void DeviceCacheImpl::RemoveFromUnpairedDeviceList(
    device::BluetoothDevice* device) {
  auto it = unpaired_devices_.begin();
  while (it != unpaired_devices_.end()) {
    if (device->GetIdentifier() == (*it)->device_properties->id) {
      unpaired_devices_.erase(it);
      NotifyUnpairedDevicesListChanged();
      return;
    }

    ++it;
  }
}

void DeviceCacheImpl::AttemptUpdateUnpairedDeviceMetadata(
    device::BluetoothDevice* device) {
  // Remove existing metadata about |device|.
  RemoveFromUnpairedDeviceList(device);

  // Now, add updated metadata.
  AttemptSetDeviceInUnpairedDeviceList(device);
}

void DeviceCacheImpl::SortUnpairedDeviceList() {
  std::sort(
      unpaired_devices_.begin(), unpaired_devices_.end(),
      [](const std::unique_ptr<UnpairedDevice>& first,
         const std::unique_ptr<UnpairedDevice>& second) {
        int8_t first_inquiry_rssi = first->inquiry_rssi.has_value()
                                        ? first->inquiry_rssi.value()
                                        : std::numeric_limits<int8_t>::min();
        int8_t second_inquiry_rssi = second->inquiry_rssi.has_value()
                                         ? second->inquiry_rssi.value()
                                         : std::numeric_limits<int8_t>::min();
        // A higher RSSI value means a stronger signal.
        return first_inquiry_rssi > second_inquiry_rssi;
      });
}

}  // namespace bluetooth_config
}  // namespace chromeos
