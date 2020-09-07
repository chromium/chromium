// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_ENTERPRISE_ARC_DATA_SNAPSHOTD_MANAGER_H_
#define COMPONENTS_ARC_ENTERPRISE_ARC_DATA_SNAPSHOTD_MANAGER_H_

#include <memory>

#include "base/memory/weak_ptr.h"

namespace arc {
namespace data_snapshotd {

class ArcDataSnapshotdBridge;

// This class manages ARC data/ directory snapshots and controls the lifetime of
// the arc-data-snapshotd daemon.
class ArcDataSnapshotdManager final {
 public:
  ArcDataSnapshotdManager();
  ArcDataSnapshotdManager(const ArcDataSnapshotdManager&) = delete;
  ArcDataSnapshotdManager& operator=(const ArcDataSnapshotdManager&) = delete;
  ~ArcDataSnapshotdManager();

  // Starts arc-data-snapshotd.
  void StartDaemon();
  // Stops arc-data-snapshotd.
  void StopDaemon();

  // Get |bridge_| for testing.
  ArcDataSnapshotdBridge* bridge() { return bridge_.get(); }

 private:
  void OnDaemonStarted(bool success);
  void OnDaemonStopped(bool success);

  std::unique_ptr<ArcDataSnapshotdBridge> bridge_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ArcDataSnapshotdManager> weak_ptr_factory_{this};
};

}  // namespace data_snapshotd
}  // namespace arc

#endif  // COMPONENTS_ARC_ENTERPRISE_ARC_DATA_SNAPSHOTD_MANAGER_H_
