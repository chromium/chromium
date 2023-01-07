// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/fake_synchronous_shutdown_object_container.h"

namespace ash {

namespace tether {

FakeSynchronousShutdownObjectContainer::FakeSynchronousShutdownObjectContainer(
    base::OnceClosure deletion_callback)
    : deletion_callback_(std::move(deletion_callback)) {}

FakeSynchronousShutdownObjectContainer::
    ~FakeSynchronousShutdownObjectContainer() {
  std::move(deletion_callback_).Run();
}

ActiveHost* FakeSynchronousShutdownObjectContainer::active_host() {
  return active_host_;
}

HostScanCache* FakeSynchronousShutdownObjectContainer::host_scan_cache() {
  return host_scan_cache_;
}

HostScanScheduler*
FakeSynchronousShutdownObjectContainer::host_scan_scheduler() {
  return host_scan_scheduler_;
}

TetherDisconnector*
FakeSynchronousShutdownObjectContainer::tether_disconnector() {
  return tether_disconnector_;
}

}  // namespace tether

}  // namespace ash
