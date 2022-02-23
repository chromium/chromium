// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/discovered_devices_provider.h"

namespace chromeos {
namespace bluetooth_config {

DiscoveredDevicesProvider::DiscoveredDevicesProvider() = default;

DiscoveredDevicesProvider::~DiscoveredDevicesProvider() = default;

void DiscoveredDevicesProvider::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DiscoveredDevicesProvider::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void DiscoveredDevicesProvider::NotifyDiscoveredDevicesListChanged() {
  for (auto& observer : observers_)
    observer.OnDiscoveredDevicesListChanged();
}

}  // namespace bluetooth_config
}  // namespace chromeos
