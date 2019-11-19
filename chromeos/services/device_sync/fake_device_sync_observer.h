// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_DEVICE_SYNC_OBSERVER_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_DEVICE_SYNC_OBSERVER_H_

#include "base/macros.h"
#include "chromeos/services/device_sync/public/mojom/device_sync.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace chromeos {

namespace device_sync {

// Fake DeviceSyncObserver implementation for tests.
class FakeDeviceSyncObserver : public mojom::DeviceSyncObserver {
 public:
  FakeDeviceSyncObserver();
  ~FakeDeviceSyncObserver() override;

  mojo::PendingRemote<mojom::DeviceSyncObserver> GenerateRemote();

  size_t num_enrollment_events() { return num_enrollment_events_; }
  size_t num_sync_events() { return num_sync_events_; }

  // mojom::DeviceSyncObserver:
  void OnEnrollmentFinished() override;
  void OnNewDevicesSynced() override;

 private:
  size_t num_enrollment_events_ = 0u;
  size_t num_sync_events_ = 0u;

  mojo::ReceiverSet<mojom::DeviceSyncObserver> receivers_;

  DISALLOW_COPY_AND_ASSIGN(FakeDeviceSyncObserver);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_DEVICE_SYNC_OBSERVER_H_
