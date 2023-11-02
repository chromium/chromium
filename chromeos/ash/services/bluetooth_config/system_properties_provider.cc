// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/system_properties_provider.h"

namespace ash::bluetooth_config {

SystemPropertiesProvider::SystemPropertiesProvider() = default;

SystemPropertiesProvider::~SystemPropertiesProvider() = default;

void SystemPropertiesProvider::Observe(
    mojo::PendingRemote<mojom::SystemPropertiesObserver> observer) {
  mojo::RemoteSetElementId id = observers_.Add(std::move(observer));
  NotifyObserver(observers_.Get(id), GenerateProperties());
}

void SystemPropertiesProvider::NotifyPropertiesChanged() {
  mojom::BluetoothSystemPropertiesPtr properties = GenerateProperties();

  for (auto& observer : observers_)
    NotifyObserver(observer.get(), properties.Clone());
}

mojom::BluetoothSystemPropertiesPtr
SystemPropertiesProvider::GenerateProperties() {
  auto properties = mojom::BluetoothSystemProperties::New();
  properties->system_state = ComputeSystemState();
  properties->modification_state = ComputeModificationState();
  properties->paired_devices = GetPairedDevices();
  return properties;
}

void SystemPropertiesProvider::FlushForTesting() {
  observers_.FlushForTesting();  // IN-TEST
}

void SystemPropertiesProvider::NotifyObserver(
    mojom::SystemPropertiesObserver* observer,
    mojom::BluetoothSystemPropertiesPtr properties) {
  observer->OnPropertiesUpdated(std::move(properties));
}

}  // namespace ash::bluetooth_config
