// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_v2_device_manager.h"

namespace ash {

namespace device_sync {

CryptAuthV2DeviceManager::CryptAuthV2DeviceManager() = default;

CryptAuthV2DeviceManager::~CryptAuthV2DeviceManager() = default;

void CryptAuthV2DeviceManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CryptAuthV2DeviceManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void CryptAuthV2DeviceManager::NotifyDeviceSyncStarted(
    const cryptauthv2::ClientMetadata& client_metadata) {
  for (auto& observer : observers_)
    observer.OnDeviceSyncStarted(client_metadata);
}

void CryptAuthV2DeviceManager::NotifyDeviceSyncFinished(
    const CryptAuthDeviceSyncResult& device_sync_result) {
  for (auto& observer : observers_)
    observer.OnDeviceSyncFinished(device_sync_result);
}

}  // namespace device_sync

}  // namespace ash
