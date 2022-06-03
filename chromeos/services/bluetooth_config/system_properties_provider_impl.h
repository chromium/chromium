// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_BLUETOOTH_CONFIG_SYSTEM_PROPERTIES_PROVIDER_IMPL_H_
#define CHROMEOS_SERVICES_BLUETOOTH_CONFIG_SYSTEM_PROPERTIES_PROVIDER_IMPL_H_

#include "base/scoped_observation.h"
#include "chromeos/services/bluetooth_config/adapter_state_controller.h"
#include "chromeos/services/bluetooth_config/device_cache.h"
#include "chromeos/services/bluetooth_config/system_properties_provider.h"
#include "components/session_manager/core/session_manager_observer.h"

namespace chromeos {
namespace bluetooth_config {

// SystemPropertiesProvider implementation which uses AdapterStateController as
// the source of properties.
class SystemPropertiesProviderImpl
    : public SystemPropertiesProvider,
      public AdapterStateController::Observer,
      public session_manager::SessionManagerObserver,
      public DeviceCache::Observer {
 public:
  SystemPropertiesProviderImpl(AdapterStateController* adapter_state_controller,
                               DeviceCache* device_cache);
  ~SystemPropertiesProviderImpl() override;

 private:
  friend class SystemPropertiesProviderImplTest;

  // SystemPropertiesProvider:
  mojom::BluetoothSystemState ComputeSystemState() const override;
  mojom::BluetoothModificationState ComputeModificationState() const override;
  std::vector<mojom::PairedBluetoothDevicePropertiesPtr> GetPairedDevices()
      const override;

  // AdapterStateController::Observer:
  void OnAdapterStateChanged() override;

  // SessionManagerObserver:
  void OnSessionStateChanged() override;

  // DeviceCache::Observer:
  void OnPairedDevicesListChanged() override;

  AdapterStateController* adapter_state_controller_;
  DeviceCache* device_cache_;

  base::ScopedObservation<AdapterStateController,
                          AdapterStateController::Observer>
      adapter_state_controller_observation_{this};
  base::ScopedObservation<DeviceCache, DeviceCache::Observer>
      device_cache_observation_{this};
};

}  // namespace bluetooth_config
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_BLUETOOTH_CONFIG_SYSTEM_PROPERTIES_PROVIDER_IMPL_H_
