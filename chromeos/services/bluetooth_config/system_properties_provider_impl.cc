// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/system_properties_provider_impl.h"

namespace chromeos {
namespace bluetooth_config {

SystemPropertiesProviderImpl::SystemPropertiesProviderImpl(
    AdapterStateController* adapter_state_controller,
    DeviceCache* device_cache)
    : adapter_state_controller_(adapter_state_controller),
      device_cache_(device_cache) {
  adapter_state_controller_observation_.Observe(adapter_state_controller_);
  device_cache_observation_.Observe(device_cache_);
}

SystemPropertiesProviderImpl::~SystemPropertiesProviderImpl() = default;

void SystemPropertiesProviderImpl::OnAdapterStateChanged() {
  NotifyPropertiesChanged();
}

void SystemPropertiesProviderImpl::OnPairedDevicesListChanged() {
  NotifyPropertiesChanged();
}

mojom::BluetoothSystemState SystemPropertiesProviderImpl::ComputeSystemState()
    const {
  return adapter_state_controller_->GetAdapterState();
}

std::vector<mojom::PairedBluetoothDevicePropertiesPtr>
SystemPropertiesProviderImpl::GetPairedDevices() const {
  return device_cache_->GetPairedDevices();
}

}  // namespace bluetooth_config
}  // namespace chromeos
