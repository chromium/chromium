// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_BLUETOOTH_DEVICE_STATUS_NOTIFIER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_BLUETOOTH_DEVICE_STATUS_NOTIFIER_IMPL_H_

#include <unordered_map>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/services/bluetooth_config/bluetooth_device_status_notifier.h"
#include "chromeos/ash/services/bluetooth_config/device_cache.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace device {
class BluetoothDevice;
}  // namespace device

namespace ash::bluetooth_config {

// Concrete BluetoothDeviceStatusNotifier implementation. Uses DeviceCache to
// observe for device list changes in order to notify when a device is newly
// paired, connected or disconnected.
class BluetoothDeviceStatusNotifierImpl
    : public BluetoothDeviceStatusNotifier,
      public DeviceCache::Observer,
      public chromeos::PowerManagerClient::Observer {
 public:
  BluetoothDeviceStatusNotifierImpl(
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
      DeviceCache* device_cache,
      chromeos::PowerManagerClient* power_manager_client);
  ~BluetoothDeviceStatusNotifierImpl() override;

 private:
  friend class BluetoothDeviceStatusNotifierImplTest;

  // Time period after the Chromebook has awoken from being suspended where
  // observers are not notified of devices that have changed their connection
  // status from connected to disconnected.
  static const base::TimeDelta kSuspendCooldownTimeout;

  // DeviceCache::Observer:
  void OnPairedDevicesListChanged() override;

  // chromeos::PowerManagerClient::Observer:
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;
  void SuspendDone(base::TimeDelta sleep_duration) override;

  // Checks the paired device list to find if a device has been newly paired.
  // Notifies observers on the device state change.
  void CheckForDeviceStateChange();

  void OnSuspendCooldownTimeout();

  // Checks if device with id |device_id| is connected using a nearby share
  // connection.
  bool IsNearbyConnectionsDevice(const device::BluetoothDevice& device_id);

  // Returns a |BluetoothDevice| corresponding to |device_id|.
  device::BluetoothDevice* FindDevice(const std::string& device_id);

  // Paired devices map, maps a device id with its corresponding device
  // properties.
  std::unordered_map<std::string, mojom::PairedBluetoothDevicePropertiesPtr>
      devices_id_to_properties_map_;

  // Flag indicating that the Chromebook is currently suspended or was suspended
  // less than |kSuspendCooldownTimeout| ago.
  bool did_recently_suspend_ = false;

  // Timer that starts once the Chromebook has awoken from being suspended that
  // expires after |kSuspendCooldownTimeout|.
  base::OneShotTimer suspend_cooldown_timer_;

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;
  raw_ptr<DeviceCache> device_cache_;

  raw_ptr<chromeos::PowerManagerClient> power_manager_client_;

  base::ScopedObservation<DeviceCache, DeviceCache::Observer>
      device_cache_observation_{this};

  base::ScopedObservation<chromeos::PowerManagerClient,
                          chromeos::PowerManagerClient::Observer>
      power_manager_client_observation_{this};
};

}  // namespace ash::bluetooth_config

#endif  // CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_BLUETOOTH_DEVICE_STATUS_NOTIFIER_IMPL_H_
