// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/fake_device_sync_observer.h"

namespace ash {

namespace device_sync {

FakeDeviceSyncObserver::FakeDeviceSyncObserver() = default;

FakeDeviceSyncObserver::~FakeDeviceSyncObserver() = default;

mojo::PendingRemote<mojom::DeviceSyncObserver>
FakeDeviceSyncObserver::GenerateRemote() {
  mojo::PendingRemote<mojom::DeviceSyncObserver> remote;
  receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void FakeDeviceSyncObserver::OnEnrollmentFinished() {
  ++num_enrollment_events_;
}

void FakeDeviceSyncObserver::OnNewDevicesSynced() {
  ++num_sync_events_;
}

}  // namespace device_sync

}  // namespace ash
