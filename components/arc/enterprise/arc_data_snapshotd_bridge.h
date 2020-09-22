// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_ENTERPRISE_ARC_DATA_SNAPSHOTD_BRIDGE_H_
#define COMPONENTS_ARC_ENTERPRISE_ARC_DATA_SNAPSHOTD_BRIDGE_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"

namespace arc {
namespace data_snapshotd {

// This class is responsible for bootstrapping D-Bus communication with
// arc-data-snapshotd daemon and delegating all ARC data/ snapshot related
// operations to it.
class ArcDataSnapshotdBridge {
 public:
  explicit ArcDataSnapshotdBridge(
      base::OnceClosure on_bridge_available_callback);
  ArcDataSnapshotdBridge(const ArcDataSnapshotdBridge&) = delete;
  ArcDataSnapshotdBridge& operator=(const ArcDataSnapshotdBridge&) = delete;
  ~ArcDataSnapshotdBridge();

  static base::TimeDelta connection_attempt_interval_for_testing();
  static int max_connection_attempt_count_for_testing();

  // Delegates the key pair generation to arc-data-snapshotd daemon.
  void GenerateKeyPair(base::OnceCallback<void(bool)> callback);
  void ClearSnapshot(bool last, base::OnceCallback<void(bool)> callback);

  bool is_available_for_testing() { return is_available_; }

 private:
  // Starts waiting until the arc-data-snapshotd D-Bus service becomes available
  // (or until this waiting fails).
  void WaitForDBusService();
  // Schedules a postponed execution of WaitForDBusService().
  void ScheduleWaitingForDBusService();
  // Called once waiting for the D-Bus service, started by WaitForDBusService(),
  // finishes.
  void OnWaitedForDBusService(bool service_is_available);

  // Callback passed in constructor and called once the D-Bus bridge is set up
  // or the number of max attempts exceeded.
  base::OnceClosure on_bridge_available_callback_;

  // Current consecutive connection attempt number.
  int connection_attempt_ = 0;
  // True if D-Bus service is available.
  bool is_available_ = false;

  // Used for cancelling previously posted tasks that wait for the D-Bus service
  // availability.
  base::WeakPtrFactory<ArcDataSnapshotdBridge> dbus_waiting_weak_ptr_factory_{
      this};
  // WeakPtrFactory to use for callbacks.
  base::WeakPtrFactory<ArcDataSnapshotdBridge> weak_ptr_factory_{this};
};

}  // namespace data_snapshotd
}  // namespace arc

#endif  // COMPONENTS_ARC_ENTERPRISE_ARC_DATA_SNAPSHOTD_BRIDGE_H_
