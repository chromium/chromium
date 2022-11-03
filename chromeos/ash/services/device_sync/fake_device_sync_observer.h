// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_DEVICE_SYNC_OBSERVER_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_DEVICE_SYNC_OBSERVER_H_

#include "chromeos/ash/services/device_sync/public/mojom/device_sync.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash {

namespace device_sync {

// Fake DeviceSyncObserver implementation for tests.
class FakeDeviceSyncObserver : public mojom::DeviceSyncObserver {
 public:
  FakeDeviceSyncObserver();

  FakeDeviceSyncObserver(const FakeDeviceSyncObserver&) = delete;
  FakeDeviceSyncObserver& operator=(const FakeDeviceSyncObserver&) = delete;

  ~FakeDeviceSyncObserver() override;

  mojo::PendingRemote<mojom::DeviceSyncObserver> GenerateRemote();

  size_t num_enrollment_events() { return num_enrollment_events_; }
  size_t num_sync_events() { return num_sync_events_; }

  // device_sync::mojom::DeviceSyncObserver:
  void OnEnrollmentFinished() override;
  void OnNewDevicesSynced() override;

 private:
  size_t num_enrollment_events_ = 0u;
  size_t num_sync_events_ = 0u;

  mojo::ReceiverSet<mojom::DeviceSyncObserver> receivers_;
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_DEVICE_SYNC_OBSERVER_H_
