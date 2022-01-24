// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_BLUETOOTH_CONFIG_BLUETOOTH_DEVICE_STATUS_NOTIFIER_IMPL_H_
#define CHROMEOS_SERVICES_BLUETOOTH_CONFIG_BLUETOOTH_DEVICE_STATUS_NOTIFIER_IMPL_H_

#include <unordered_map>

#include "base/scoped_observation.h"
#include "chromeos/services/bluetooth_config/bluetooth_device_status_notifier.h"
#include "chromeos/services/bluetooth_config/device_cache.h"

namespace chromeos {
namespace bluetooth_config {

// Concrete BluetoothDeviceStatusNotifier implementation. Uses DeviceCache to
// observe for device list changes in order to notify when a device is newly
// paired, connected or disconnected.
class BluetoothDeviceStatusNotifierImpl : public BluetoothDeviceStatusNotifier,
                                          public DeviceCache::Observer {
 public:
  explicit BluetoothDeviceStatusNotifierImpl(DeviceCache* device_cache);
  ~BluetoothDeviceStatusNotifierImpl() override;

 private:
  // DeviceCache::Observer:
  void OnPairedDevicesListChanged() override;

  // Checks the paired device list to find if a device has been newly paired.
  // Notifies observers on the device state change.
  void CheckForDeviceStateChange();

  // Paired devices map, maps a device id with its corresponding device
  // properties.
  std::unordered_map<std::string, mojom::PairedBluetoothDevicePropertiesPtr>
      devices_id_to_properties_map_;

  DeviceCache* device_cache_;

  base::ScopedObservation<DeviceCache, DeviceCache::Observer>
      device_cache_observation_{this};
};

}  // namespace bluetooth_config
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_BLUETOOTH_CONFIG_BLUETOOTH_DEVICE_STATUS_NOTIFIER_IMPL_H_
