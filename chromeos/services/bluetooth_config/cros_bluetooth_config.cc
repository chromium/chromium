// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/cros_bluetooth_config.h"

#include "chromeos/services/bluetooth_config/system_properties_provider_impl.h"

namespace chromeos {
namespace bluetooth_config {

CrosBluetoothConfig::CrosBluetoothConfig(
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter)
    : system_properties_provider_(
          std::make_unique<SystemPropertiesProviderImpl>(bluetooth_adapter)) {}

CrosBluetoothConfig::~CrosBluetoothConfig() = default;

void CrosBluetoothConfig::BindPendingReceiver(
    mojo::PendingReceiver<mojom::CrosBluetoothConfig> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void CrosBluetoothConfig::ObserveSystemProperties(
    mojo::PendingRemote<mojom::SystemPropertiesObserver> observer) {
  system_properties_provider_->Observe(std::move(observer));
}

}  // namespace bluetooth_config
}  // namespace chromeos
