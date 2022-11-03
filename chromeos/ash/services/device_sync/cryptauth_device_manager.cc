// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_device_manager.h"

#include "chromeos/ash/services/device_sync/pref_names.h"
#include "components/prefs/pref_registry_simple.h"

namespace ash {

namespace device_sync {

// static
void CryptAuthDeviceManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDoublePref(prefs::kCryptAuthDeviceSyncLastSyncTimeSeconds,
                               0.0);
  registry->RegisterBooleanPref(
      prefs::kCryptAuthDeviceSyncIsRecoveringFromFailure, false);
  registry->RegisterIntegerPref(prefs::kCryptAuthDeviceSyncReason,
                                cryptauth::INVOCATION_REASON_UNKNOWN);
  registry->RegisterListPref(prefs::kCryptAuthDeviceSyncUnlockKeys);
}

CryptAuthDeviceManager::CryptAuthDeviceManager() = default;

CryptAuthDeviceManager::~CryptAuthDeviceManager() = default;

void CryptAuthDeviceManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CryptAuthDeviceManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void CryptAuthDeviceManager::NotifySyncStarted() {
  for (auto& observer : observers_)
    observer.OnSyncStarted();
}

void CryptAuthDeviceManager::NotifySyncFinished(
    SyncResult sync_result,
    DeviceChangeResult device_change_result) {
  for (auto& observer : observers_)
    observer.OnSyncFinished(sync_result, device_change_result);
}

}  // namespace device_sync

}  // namespace ash
