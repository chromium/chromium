// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/fake_synced_bluetooth_address_tracker.h"

namespace ash {

namespace device_sync {

FakeSyncedBluetoothAddressTracker::FakeSyncedBluetoothAddressTracker() =
    default;

FakeSyncedBluetoothAddressTracker::~FakeSyncedBluetoothAddressTracker() =
    default;

void FakeSyncedBluetoothAddressTracker::GetBluetoothAddress(
    BluetoothAddressCallback callback) {
  std::move(callback).Run(bluetooth_address_);
}

void FakeSyncedBluetoothAddressTracker::SetLastSyncedBluetoothAddress(
    const std::string& last_synced_bluetooth_address) {
  last_synced_bluetooth_address_ = last_synced_bluetooth_address;
}

FakeSyncedBluetoothAddressTrackerFactory::
    FakeSyncedBluetoothAddressTrackerFactory() = default;

FakeSyncedBluetoothAddressTrackerFactory::
    ~FakeSyncedBluetoothAddressTrackerFactory() = default;

std::unique_ptr<SyncedBluetoothAddressTracker>
FakeSyncedBluetoothAddressTrackerFactory::CreateInstance(
    CryptAuthScheduler* cryptauth_scheduler,
    PrefService* pref_service) {
  auto instance = std::make_unique<FakeSyncedBluetoothAddressTracker>();
  last_created_ = instance.get();
  return instance;
}

}  // namespace device_sync

}  // namespace ash
