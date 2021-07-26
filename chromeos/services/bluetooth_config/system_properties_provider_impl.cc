// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/system_properties_provider_impl.h"

namespace chromeos {
namespace bluetooth_config {

SystemPropertiesProviderImpl::SystemPropertiesProviderImpl(
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter)
    : bluetooth_adapter_(std::move(bluetooth_adapter)) {
  adapter_observation_.Observe(bluetooth_adapter_.get());
}

SystemPropertiesProviderImpl::~SystemPropertiesProviderImpl() = default;

mojom::BluetoothSystemPropertiesPtr
SystemPropertiesProviderImpl::GenerateProperties() {
  auto properties = mojom::BluetoothSystemProperties::New();
  properties->system_state = ComputeSystemState();
  return properties;
}

void SystemPropertiesProviderImpl::AdapterPresentChanged(
    device::BluetoothAdapter* adapter,
    bool present) {
  NotifyPropertiesChanged();
}

void SystemPropertiesProviderImpl::AdapterPoweredChanged(
    device::BluetoothAdapter* adapter,
    bool powered) {
  NotifyPropertiesChanged();
}

mojom::BluetoothSystemState SystemPropertiesProviderImpl::ComputeSystemState()
    const {
  if (!bluetooth_adapter_->IsPresent())
    return mojom::BluetoothSystemState::kUnavailable;

  return bluetooth_adapter_->IsPowered()
             ? mojom::BluetoothSystemState::kEnabled
             : mojom::BluetoothSystemState::kDisabled;
}

}  // namespace bluetooth_config
}  // namespace chromeos
