// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/cros_bluetooth_config.h"

#include "chromeos/services/bluetooth_config/adapter_state_controller_impl.h"
#include "chromeos/services/bluetooth_config/system_properties_provider_impl.h"

namespace chromeos {
namespace bluetooth_config {

CrosBluetoothConfig::CrosBluetoothConfig(
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter)
    : adapter_state_controller_(
          std::make_unique<AdapterStateControllerImpl>(bluetooth_adapter)),
      system_properties_provider_(
          std::make_unique<SystemPropertiesProviderImpl>(
              adapter_state_controller_.get())) {}

CrosBluetoothConfig::~CrosBluetoothConfig() = default;

void CrosBluetoothConfig::BindPendingReceiver(
    mojo::PendingReceiver<mojom::CrosBluetoothConfig> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void CrosBluetoothConfig::ObserveSystemProperties(
    mojo::PendingRemote<mojom::SystemPropertiesObserver> observer) {
  system_properties_provider_->Observe(std::move(observer));
}

void CrosBluetoothConfig::SetBluetoothEnabledState(bool enabled) {
  adapter_state_controller_->SetBluetoothEnabledState(enabled);
}

}  // namespace bluetooth_config
}  // namespace chromeos
