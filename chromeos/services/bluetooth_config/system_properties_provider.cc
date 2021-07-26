// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/system_properties_provider.h"

namespace chromeos {
namespace bluetooth_config {

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

void SystemPropertiesProvider::NotifyObserver(
    mojom::SystemPropertiesObserver* observer,
    mojom::BluetoothSystemPropertiesPtr properties) {
  observer->OnPropertiesUpdated(std::move(properties));
}

void SystemPropertiesProvider::FlushForTesting() {
  observers_.FlushForTesting();  // IN-TEST
}

}  // namespace bluetooth_config
}  // namespace chromeos
