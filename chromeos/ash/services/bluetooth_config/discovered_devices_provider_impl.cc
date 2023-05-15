// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/discovered_devices_provider_impl.h"

#include "base/ranges/algorithm.h"

namespace ash::bluetooth_config {

// static
const base::TimeDelta DiscoveredDevicesProviderImpl::kNotificationDelay =
    base::Seconds(5);

DiscoveredDevicesProviderImpl::DiscoveredDevicesProviderImpl(
    DeviceCache* device_cache)
    : device_cache_(device_cache) {
  device_cache_observation_.Observe(device_cache_.get());
}

DiscoveredDevicesProviderImpl::~DiscoveredDevicesProviderImpl() = default;

std::vector<mojom::BluetoothDevicePropertiesPtr>
DiscoveredDevicesProviderImpl::GetDiscoveredDevices() const {
  std::vector<mojom::BluetoothDevicePropertiesPtr> discovered_devices;
  for (const auto& discovered_device : discovered_devices_)
    discovered_devices.push_back(discovered_device->Clone());
  return discovered_devices;
}

void DiscoveredDevicesProviderImpl::OnUnpairedDevicesListChanged() {
  // When the list of unpaired devices changes, this method implements the
  // following logic in effort to limit the frequency of device position changes
  // clients observe:
  //
  // If a device has been added, it's appended to the end of
  // the list and clients are notified. If no timer is currently running, after
  // |kNotificationDelay|, the list is sorted, and clients are notified again.
  //
  // If a device has been updated, it's properties are updated but its position
  // un-updated, and clients are notified. If no timer is currently running,
  // after |kNotificationDelay|, the list is sorted, and clients are notified
  // again.
  //
  // If a device has been removed, it's removed from the list and clients are
  // notified. If no timer is currently running, after
  // |kNotificationDelay|, the list is sorted, and clients are notified again.
  // This last sorting and notification are unnecessary but simplify this
  // method.
  std::vector<mojom::BluetoothDevicePropertiesPtr> unpaired_devices =
      device_cache_->GetUnpairedDevices();
  std::unordered_set<std::string> unpaired_device_ids;

  // Iterate through |unpaired_devices| to find which devices have been added
  // and which have been updated.
  for (const auto& unpaired_device : unpaired_devices) {
    // Save |unpaired_device|'s id for later to check for device removals.
    unpaired_device_ids.insert(unpaired_device->id);

    // Check if |discovered_devices_| contains |unpaired_device_|.
    const auto it = base::ranges::find(discovered_devices_, unpaired_device->id,
                                       &mojom::BluetoothDeviceProperties::id);
    if (it == discovered_devices_.end()) {
      // If it doesn't contain the device, the device was added. Append to end
      // of the list.
      discovered_devices_.push_back(unpaired_device.Clone());
    } else {
      // If it does contain the device, update the device.
      *it = unpaired_device.Clone();
    }
  }

  // Remove any devices in |discovered_devices_| that are not found in
  // |unpaired_devices|.
  discovered_devices_.erase(
      std::remove_if(discovered_devices_.begin(), discovered_devices_.end(),
                     [&unpaired_device_ids](const auto& discovered_device) {
                       return unpaired_device_ids.find(discovered_device->id) ==
                              unpaired_device_ids.end();
                     }),
      discovered_devices_.end());

  // Immediately notify clients of changes before sorting.
  NotifyDiscoveredDevicesListChanged();

  // Don't set the timer if one is already running.
  if (notification_delay_timer_.IsRunning())
    return;

  // Wait |kNotificationDelay| to sort the list and inform clients that the list
  // of discovered devices has changed.
  notification_delay_timer_.Start(
      FROM_HERE, kNotificationDelay,
      base::BindOnce(
          &DiscoveredDevicesProviderImpl::SortDiscoveredDevicesAndNotify,
          weak_ptr_factory_.GetWeakPtr()));
}

void DiscoveredDevicesProviderImpl::SortDiscoveredDevicesAndNotify() {
  // |device_cache_| will have the most recent list of sorted discovered
  // devices. Copy this over to |discovered_devices_| instead of needing to sort
  // it ourselves.
  discovered_devices_.clear();
  for (const auto& unpaired_device : device_cache_->GetUnpairedDevices())
    discovered_devices_.push_back(unpaired_device.Clone());
  NotifyDiscoveredDevicesListChanged();
}

}  // namespace ash::bluetooth_config
