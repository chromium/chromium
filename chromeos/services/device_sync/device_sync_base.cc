// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/device_sync_base.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"

namespace chromeos {

namespace device_sync {

DeviceSyncBase::DeviceSyncBase() {
  receivers_.set_disconnect_handler(base::BindRepeating(
      &DeviceSyncBase::OnDisconnection, base::Unretained(this)));
}

DeviceSyncBase::~DeviceSyncBase() = default;

void DeviceSyncBase::AddObserver(
    mojo::PendingRemote<mojom::DeviceSyncObserver> observer,
    AddObserverCallback callback) {
  observers_.Add(std::move(observer));
  std::move(callback).Run();
}

void DeviceSyncBase::BindReceiver(
    mojo::PendingReceiver<mojom::DeviceSync> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void DeviceSyncBase::CloseAllReceivers() {
  receivers_.Clear();
}

void DeviceSyncBase::NotifyOnEnrollmentFinished() {
  for (auto& observer : observers_)
    observer->OnEnrollmentFinished();
}

void DeviceSyncBase::NotifyOnNewDevicesSynced() {
  for (auto& observer : observers_)
    observer->OnNewDevicesSynced();
}

void DeviceSyncBase::OnDisconnection() {
  // If all clients have disconnected, shut down.
  if (receivers_.empty())
    Shutdown();
}

}  // namespace device_sync

}  // namespace chromeos
