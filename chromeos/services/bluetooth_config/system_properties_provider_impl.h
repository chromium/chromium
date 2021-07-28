// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_BLUETOOTH_CONFIG_SYSTEM_PROPERTIES_PROVIDER_IMPL_H_
#define CHROMEOS_SERVICES_BLUETOOTH_CONFIG_SYSTEM_PROPERTIES_PROVIDER_IMPL_H_

#include "base/scoped_observation.h"
#include "chromeos/services/bluetooth_config/adapter_state_controller.h"
#include "chromeos/services/bluetooth_config/system_properties_provider.h"

namespace chromeos {
namespace bluetooth_config {

// SystemPropertiesProvider implementation which uses AdapterStateController as
// the source of properties.
class SystemPropertiesProviderImpl : public SystemPropertiesProvider,
                                     public AdapterStateController::Observer {
 public:
  explicit SystemPropertiesProviderImpl(
      AdapterStateController* adapter_state_controller);
  ~SystemPropertiesProviderImpl() override;

 private:
  friend class SystemPropertiesProviderImplTest;

  // SystemPropertiesProvider:
  mojom::BluetoothSystemState ComputeSystemState() const override;

  // AdapterStateController::Observer:
  void OnAdapterStateChanged() override;

  AdapterStateController* adapter_state_controller_;
  base::ScopedObservation<AdapterStateController,
                          AdapterStateController::Observer>
      adapter_state_controller_observation_{this};
};

}  // namespace bluetooth_config
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_BLUETOOTH_CONFIG_SYSTEM_PROPERTIES_PROVIDER_IMPL_H_
