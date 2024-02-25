// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_SYNCHRONOUS_SHUTDOWN_OBJECT_CONTAINER_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_SYNCHRONOUS_SHUTDOWN_OBJECT_CONTAINER_H_

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/tether/synchronous_shutdown_object_container.h"

namespace ash {

namespace tether {

// Test double for SynchronousShutdownObjectContainer.
class FakeSynchronousShutdownObjectContainer
    : public SynchronousShutdownObjectContainer {
 public:
  // |deletion_callback| will be invoked when the object is deleted.
  FakeSynchronousShutdownObjectContainer(
      base::OnceClosure deletion_callback = base::DoNothing());

  FakeSynchronousShutdownObjectContainer(
      const FakeSynchronousShutdownObjectContainer&) = delete;
  FakeSynchronousShutdownObjectContainer& operator=(
      const FakeSynchronousShutdownObjectContainer&) = delete;

  ~FakeSynchronousShutdownObjectContainer() override;

  void set_active_host(ActiveHost* active_host) { active_host_ = active_host; }

  void set_host_scan_cache(HostScanCache* host_scan_cache) {
    host_scan_cache_ = host_scan_cache;
  }

  void set_host_scan_scheduler(HostScanScheduler* host_scan_scheduler) {
    host_scan_scheduler_ = host_scan_scheduler;
  }

  void set_tether_disconnector(TetherDisconnector* tether_disconnector) {
    tether_disconnector_ = tether_disconnector;
  }

  // SynchronousShutdownObjectContainer:
  ActiveHost* active_host() override;
  HostScanCache* host_scan_cache() override;
  HostScanScheduler* host_scan_scheduler() override;
  TetherDisconnector* tether_disconnector() override;

 private:
  base::OnceClosure deletion_callback_;

  raw_ptr<ActiveHost> active_host_ = nullptr;
  raw_ptr<HostScanCache> host_scan_cache_ = nullptr;
  raw_ptr<HostScanScheduler> host_scan_scheduler_ = nullptr;
  raw_ptr<TetherDisconnector> tether_disconnector_ = nullptr;
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_SYNCHRONOUS_SHUTDOWN_OBJECT_CONTAINER_H_
