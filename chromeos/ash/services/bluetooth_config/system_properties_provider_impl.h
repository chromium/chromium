// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_SYSTEM_PROPERTIES_PROVIDER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_SYSTEM_PROPERTIES_PROVIDER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/services/bluetooth_config/adapter_state_controller.h"
#include "chromeos/ash/services/bluetooth_config/device_cache.h"
#include "chromeos/ash/services/bluetooth_config/fast_pair_delegate.h"
#include "chromeos/ash/services/bluetooth_config/system_properties_provider.h"
#include "components/session_manager/core/session_manager_observer.h"

namespace ash::bluetooth_config {

// SystemPropertiesProvider implementation which uses AdapterStateController as
// the source of properties.
class SystemPropertiesProviderImpl
    : public SystemPropertiesProvider,
      public AdapterStateController::Observer,
      public session_manager::SessionManagerObserver,
      public DeviceCache::Observer,
      public FastPairDelegate::Observer {
 public:
  SystemPropertiesProviderImpl(AdapterStateController* adapter_state_controller,
                               DeviceCache* device_cache,
                               FastPairDelegate* fast_pair_delegate);
  ~SystemPropertiesProviderImpl() override;

 private:
  friend class SystemPropertiesProviderImplTest;

  // SystemPropertiesProvider:
  mojom::BluetoothSystemState ComputeSystemState() const override;
  mojom::BluetoothModificationState ComputeModificationState() const override;
  std::vector<mojom::PairedBluetoothDevicePropertiesPtr> GetPairedDevices()
      const override;
  std::vector<mojom::PairedBluetoothDevicePropertiesPtr>
  GetFastPairableDevices() const override;

  // AdapterStateController::Observer:
  void OnAdapterStateChanged() override;

  // SessionManagerObserver:
  void OnSessionStateChanged() override;

  // DeviceCache::Observer:
  void OnPairedDevicesListChanged() override;

  // FastPairDelegate::Observer
  void OnFastPairableDevicesChanged(
      const std::vector<mojom::PairedBluetoothDevicePropertiesPtr>&
          fast_pairable_devices) override;

  raw_ptr<AdapterStateController> adapter_state_controller_;
  raw_ptr<DeviceCache> device_cache_;
  raw_ptr<FastPairDelegate> fast_pair_delegate_;

  base::ScopedObservation<AdapterStateController,
                          AdapterStateController::Observer>
      adapter_state_controller_observation_{this};
  base::ScopedObservation<DeviceCache, DeviceCache::Observer>
      device_cache_observation_{this};
  base::ScopedObservation<FastPairDelegate, FastPairDelegate::Observer>
      fast_pair_delegate_observation_{this};
};

}  // namespace ash::bluetooth_config

#endif  // CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_SYSTEM_PROPERTIES_PROVIDER_IMPL_H_
