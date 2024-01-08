// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_DISCOVERED_DEVICES_PROVIDER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_DISCOVERED_DEVICES_PROVIDER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/bluetooth_config/discovered_devices_provider.h"

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/services/bluetooth_config/device_cache.h"

namespace ash::bluetooth_config {

// Concrete DiscoveredDevicesProvider implementation that batches discovered
// devices list updates. If the device list has changed, this implementation
// waits |kNotificationDelay| before sorting and notifying clients that the list
// has changed. This is to reduce the frequency of changes to the device list in
// UI surfaces, giving users more time to view the list between updates.
class DiscoveredDevicesProviderImpl : public DiscoveredDevicesProvider,
                                      public DeviceCache::Observer {
 public:
  explicit DiscoveredDevicesProviderImpl(DeviceCache* device_cache);
  ~DiscoveredDevicesProviderImpl() override;

 private:
  friend class DiscoveredDevicesProviderImplTest;

  // Delay from when the unpaired devices list has changed and when clients are
  // notified.
  static const base::TimeDelta kNotificationDelay;

  // DiscoveredDevicesProvider:
  std::vector<mojom::BluetoothDevicePropertiesPtr> GetDiscoveredDevices()
      const override;

  // DeviceCache::Observer:
  void OnUnpairedDevicesListChanged() override;

  // Method invoked once |notification_delay_timer_| expires that sorts
  // |discovered_devices_|, then notifies clients of the change.
  void SortDiscoveredDevicesAndNotify();

  base::OneShotTimer notification_delay_timer_;

  std::vector<mojom::BluetoothDevicePropertiesPtr> discovered_devices_;

  raw_ptr<DeviceCache> device_cache_;

  base::ScopedObservation<DeviceCache, DeviceCache::Observer>
      device_cache_observation_{this};

  base::WeakPtrFactory<DiscoveredDevicesProviderImpl> weak_ptr_factory_{this};
};

}  // namespace ash::bluetooth_config

#endif  // CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_DISCOVERED_DEVICES_PROVIDER_IMPL_H_
