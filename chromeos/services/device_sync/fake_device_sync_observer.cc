// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/fake_device_sync_observer.h"

namespace chromeos {

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

}  // namespace chromeos
