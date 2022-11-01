// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/public/cpp/device_sync_client.h"

#include "chromeos/ash/components/multidevice/logging/logging.h"

namespace ash {

namespace device_sync {

DeviceSyncClient::DeviceSyncClient() = default;

DeviceSyncClient::~DeviceSyncClient() = default;

mojo::Remote<mojom::DeviceSync>* DeviceSyncClient::GetDeviceSyncRemote() {
  return nullptr;
}

void DeviceSyncClient::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void DeviceSyncClient::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void DeviceSyncClient::NotifyReady() {
  PA_LOG(INFO) << "Device sync is ready. Notifying observers.";
  is_ready_ = true;

  for (auto& observer : observer_list_)
    observer.OnReady();
}

void DeviceSyncClient::NotifyEnrollmentFinished() {
  for (auto& observer : observer_list_)
    observer.OnEnrollmentFinished();
}

void DeviceSyncClient::NotifyNewDevicesSynced() {
  for (auto& observer : observer_list_)
    observer.OnNewDevicesSynced();
}

}  // namespace device_sync

}  // namespace ash
