// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/enterprise/arc_data_snapshotd_bridge.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/dbus/arc/arc_data_snapshotd_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace arc {
namespace data_snapshotd {

namespace {

// Interval between successful connection attempts.
constexpr base::TimeDelta kConnectionAttemptInterval =
    base::TimeDelta::FromSeconds(1);

// The maximum number of consecutive connection attempts before giving up.
constexpr int kMaxConnectionAttemptCount = 5;

}  // namespace

ArcDataSnapshotdBridge::ArcDataSnapshotdBridge(
    base::OnceClosure on_bridge_available_callback)
    : on_bridge_available_callback_(std::move(on_bridge_available_callback)) {
  WaitForDBusService();
}

ArcDataSnapshotdBridge::~ArcDataSnapshotdBridge() = default;

// static
base::TimeDelta
ArcDataSnapshotdBridge::connection_attempt_interval_for_testing() {
  return kConnectionAttemptInterval;
}

// static
int ArcDataSnapshotdBridge::max_connection_attempt_count_for_testing() {
  return kMaxConnectionAttemptCount;
}

void ArcDataSnapshotdBridge::WaitForDBusService() {
  if (connection_attempt_ >= kMaxConnectionAttemptCount) {
    LOG(WARNING)
        << "Stopping attempts to connect to arc-data-snapshotd - too many "
           "unsuccessful attempts in a row";
    std::move(on_bridge_available_callback_).Run();
    return;
  }
  ++connection_attempt_;

  dbus_waiting_weak_ptr_factory_.InvalidateWeakPtrs();

  chromeos::DBusThreadManager::Get()
      ->GetArcDataSnapshotdClient()
      ->WaitForServiceToBeAvailable(
          base::BindOnce(&ArcDataSnapshotdBridge::OnWaitedForDBusService,
                         dbus_waiting_weak_ptr_factory_.GetWeakPtr()));
  ScheduleWaitingForDBusService();
}

void ArcDataSnapshotdBridge::ScheduleWaitingForDBusService() {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ArcDataSnapshotdBridge::WaitForDBusService,
                     dbus_waiting_weak_ptr_factory_.GetWeakPtr()),
      kConnectionAttemptInterval);
}

void ArcDataSnapshotdBridge::OnWaitedForDBusService(bool service_is_available) {
  if (!service_is_available) {
    LOG(WARNING) << "The arc-data-snapshotd D-Bus service is unavailable";
    return;
  }

  // Cancel any tasks previously created from WaitForDBusService() or
  // ScheduleWaitingForDBusService().
  dbus_waiting_weak_ptr_factory_.InvalidateWeakPtrs();
  is_available_ = true;
  std::move(on_bridge_available_callback_).Run();
}

void ArcDataSnapshotdBridge::GenerateKeyPair(
    base::OnceCallback<void(bool)> callback) {
  if (!is_available_) {
    LOG(ERROR) << "GenerateKeyPair call when D-Bus service is not available.";
    std::move(callback).Run(false /* success */);
    return;
  }

  VLOG(1) << "GenerateKeyPair via D-Bus";
  chromeos::DBusThreadManager::Get()
      ->GetArcDataSnapshotdClient()
      ->GenerateKeyPair(std::move(callback));
}

void ArcDataSnapshotdBridge::ClearSnapshot(
    bool last,
    base::OnceCallback<void(bool)> callback) {
  if (!is_available_) {
    LOG(ERROR) << "ClearSnapshot call when D-Bus service is not available.";
    std::move(callback).Run(false /* success */);
    return;
  }
  VLOG(1) << "ClearSnapshot via D-Bus";
  // TODO(pbond): implement
  std::move(callback).Run(true /* success */);
}

}  // namespace data_snapshotd
}  // namespace arc
