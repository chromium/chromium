// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_SYNCHRONOUS_SHUTDOWN_OBJECT_CONTAINER_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_SYNCHRONOUS_SHUTDOWN_OBJECT_CONTAINER_H_

namespace ash {

namespace tether {

class ActiveHost;
class HostScanCache;
class HostScanScheduler;
class TetherDisconnector;

// Container for objects owned by the Tether component which have a
// synchronous shutdown flow (i.e., they can simply be deleted). Objects which
// may have an asynchronous shutdown flow are owned by
// AsynchronousShutdownObjectContainer.
class SynchronousShutdownObjectContainer {
 public:
  SynchronousShutdownObjectContainer() {}

  SynchronousShutdownObjectContainer(
      const SynchronousShutdownObjectContainer&) = delete;
  SynchronousShutdownObjectContainer& operator=(
      const SynchronousShutdownObjectContainer&) = delete;

  virtual ~SynchronousShutdownObjectContainer() {}

  virtual ActiveHost* active_host() = 0;
  virtual HostScanCache* host_scan_cache() = 0;
  virtual HostScanScheduler* host_scan_scheduler() = 0;
  virtual TetherDisconnector* tether_disconnector() = 0;
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_SYNCHRONOUS_SHUTDOWN_OBJECT_CONTAINER_H_
