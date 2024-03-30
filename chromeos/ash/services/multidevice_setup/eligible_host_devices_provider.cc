// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/eligible_host_devices_provider.h"

#include "chromeos/ash/components/multidevice/logging/logging.h"

namespace ash::multidevice_setup {

EligibleHostDevicesProvider::EligibleHostDevicesProvider() = default;

EligibleHostDevicesProvider::~EligibleHostDevicesProvider() = default;

void EligibleHostDevicesProvider::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void EligibleHostDevicesProvider::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void EligibleHostDevicesProvider::NotifyObserversEligibleDevicesSynced() {
  PA_LOG(INFO) << __func__ << ": Eligible Devices Synced. Notifying observers.";
  for (auto& observer : observer_list_) {
    observer.OnEligibleDevicesSynced();
  }
}

}  // namespace ash::multidevice_setup
