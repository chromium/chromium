// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/bluetooth_device_status_notifier_impl.h"

#include <vector>

#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "chromeos/ash/services/bluetooth_config/device_cache.h"
#include "chromeos/ash/services/bluetooth_config/public/cpp/cros_bluetooth_config_util.h"
#include "chromeos/ash/services/nearby/public/cpp/nearby_client_uuids.h"
#include "components/device_event_log/device_event_log.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace ash::bluetooth_config {

// static
const base::TimeDelta
    BluetoothDeviceStatusNotifierImpl::kSuspendCooldownTimeout =
        base::Milliseconds(3000);

BluetoothDeviceStatusNotifierImpl::BluetoothDeviceStatusNotifierImpl(
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
    DeviceCache* device_cache,
    chromeos::PowerManagerClient* power_manager_client)
    : bluetooth_adapter_(std::move(bluetooth_adapter)),
      device_cache_(device_cache),
      power_manager_client_(power_manager_client) {
  DCHECK(power_manager_client_);
  device_cache_observation_.Observe(device_cache_.get());
  power_manager_client_observation_.Observe(power_manager_client_.get());

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

void BluetoothDeviceStatusNotifierImpl::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  // Set |did_recently_suspend_| when the device begins suspending. It's not
  // sufficient to set this flag in SuspendDone() because
  // OnPairedDevicesListChanged() can be called before SuspendDone() when the
  // device is awoken. If the flag is not set at this point then disconnected
  // devices will be notified to observers.
  did_recently_suspend_ = true;

  // If there's a timer currently running, stop it so it doesn't timeout while
  // the device is suspended and flip |did_recently_suspend_| back to false.
  suspend_cooldown_timer_.Stop();
}

void BluetoothDeviceStatusNotifierImpl::SuspendDone(
    base::TimeDelta sleep_duration) {
  suspend_cooldown_timer_.Start(
      FROM_HERE, kSuspendCooldownTimeout,
      base::BindOnce(
          &BluetoothDeviceStatusNotifierImpl::OnSuspendCooldownTimeout,
          base::Unretained(this)));
}

void BluetoothDeviceStatusNotifierImpl::CheckForDeviceStateChange() {
  BLUETOOTH_LOG(DEBUG) << "Checking for device state changes";
  const std::vector<mojom::PairedBluetoothDevicePropertiesPtr> paired_devices =
      device_cache_->GetPairedDevices();

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

    device::BluetoothDevice* bluetooth_device =
        FindDevice(device->device_properties->id);

    if (!bluetooth_device || IsNearbyConnectionsDevice(*bluetooth_device)) {
      continue;
    }

    auto it = previous_devices_id_to_properties_map.find(
        device->device_properties->id);

    // Check if device is not in previous map and is connected. If it is not,
    // this means a new paired device was found.
    if (it == previous_devices_id_to_properties_map.end()) {
      if (device->device_properties->connection_state ==
          mojom::DeviceConnectionState::kConnected) {
        BLUETOOTH_LOG(EVENT)
            << "Device was newly paired: " << device->device_properties->id;
        NotifyDeviceNewlyPaired(device);
      }
      continue;
    }

    // Check if device is recently disconnected.
    if (it->second->device_properties->connection_state ==
            mojom::DeviceConnectionState::kConnected &&
        device->device_properties->connection_state ==
            mojom::DeviceConnectionState::kNotConnected) {
      // Check if the Chromebook is suspended or has recently awaken from being
      // suspended. If it has, do not notify observers of disconnected devices
      // (see b/216341171).
      if (did_recently_suspend_) {
        BLUETOOTH_LOG(EVENT) << "Device " << GetPairedDeviceName(device)
                             << " connection status changed to disconnected, "
                                "but device was recently awoken from being "
                                "suspended. Not notifying observers";
        continue;
      }

      BLUETOOTH_LOG(EVENT) << "Device was newly disconnected: "
                           << device->device_properties->id;
      NotifyDeviceNewlyDisconnected(device);
      continue;
    }

    // Check if device is recently connected.
    if (it->second->device_properties->connection_state !=
            mojom::DeviceConnectionState::kConnected &&
        device->device_properties->connection_state ==
            mojom::DeviceConnectionState::kConnected) {
      BLUETOOTH_LOG(EVENT) << "Device was newly connected: "
                           << device->device_properties->id;
      NotifyDeviceNewlyConnected(device);
      continue;
    }
  }

  // For some devices, when they are forgotten while still connected, they will
  // be removed from the paired devices list before their connection status is
  // updated to disconnected. To ensure observers are notified of these devices
  // disconnecting, check if there are any devices in the previous paired
  // devices list which were connected that are now missing (b/282640314).
  for (const auto& [previous_device_id, previous_device] :
       previous_devices_id_to_properties_map) {
    if (base::Contains(devices_id_to_properties_map_, previous_device_id)) {
      continue;
    }

    // Device was unpaired. Check if it was last seen as connected.
    if (previous_device->device_properties->connection_state ==
        mojom::DeviceConnectionState::kConnected) {
      BLUETOOTH_LOG(EVENT)
          << "Connected device is no longer found in paired device list, "
          << "notifying device was disconnected: "
          << previous_device->device_properties->id;
      NotifyDeviceNewlyDisconnected(previous_device);
    }
  }
}

void BluetoothDeviceStatusNotifierImpl::OnSuspendCooldownTimeout() {
  did_recently_suspend_ = false;
}

bool BluetoothDeviceStatusNotifierImpl::IsNearbyConnectionsDevice(
    const device::BluetoothDevice& device) {
  // NOTE(http://b/215024088): If the newly paired device is connected via a
  // Nearby Connections client (e.g., Nearby Share), do not display this
  // notification.
  return base::ranges::any_of(device.GetUUIDs(),
                              ash::nearby::IsNearbyClientUuid);
}

device::BluetoothDevice* BluetoothDeviceStatusNotifierImpl::FindDevice(
    const std::string& device_id) {
  for (auto* device : bluetooth_adapter_->GetDevices()) {
    if (device->GetIdentifier() == device_id)
      return device;
  }
  return nullptr;
}

}  // namespace ash::bluetooth_config
