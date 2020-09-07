// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/enterprise/arc_data_snapshotd_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/upstart/upstart_client.h"
#include "components/arc/enterprise/arc_data_snapshotd_bridge.h"

namespace arc {
namespace data_snapshotd {

namespace {

// Returns true if the Chrome session is restored after crash.
bool IsRestoredSession() {
  auto* command_line = base::CommandLine::ForCurrentProcess();

  return command_line->HasSwitch(chromeos::switches::kLoginUser) &&
         !command_line->HasSwitch(chromeos::switches::kLoginManager);
}

}  // namespace

ArcDataSnapshotdManager::ArcDataSnapshotdManager() {
  // Stop daemon, started in the previous session.
  if (IsRestoredSession())
    StopDaemon();
}

ArcDataSnapshotdManager::~ArcDataSnapshotdManager() {
  StopDaemon();
}

void ArcDataSnapshotdManager::StartDaemon() {
  VLOG(1) << "Starting arc-data-snapshotd";
  weak_ptr_factory_.InvalidateWeakPtrs();
  chromeos::UpstartClient::Get()->StartArcDataSnapshotd(
      base::BindOnce(&ArcDataSnapshotdManager::OnDaemonStarted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcDataSnapshotdManager::StopDaemon() {
  VLOG(1) << "Stopping arc-data-snapshotd";
  weak_ptr_factory_.InvalidateWeakPtrs();
  chromeos::UpstartClient::Get()->StopArcDataSnapshotd(
      base::BindOnce(&ArcDataSnapshotdManager::OnDaemonStopped,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcDataSnapshotdManager::OnDaemonStarted(bool success) {
  if (!success) {
    DLOG(ERROR) << "Failed to start arc-data-snapshotd, it might be already "
                << "running";
  } else {
    VLOG(1) << "arc-data-snapshotd started";
  }

  // The bridge has to be created regardless of a |success| value. When
  // arc-data-snapshotd is already running, it responds with an error on
  // attempt to start it.
  if (!bridge_) {
    bridge_ = std::make_unique<ArcDataSnapshotdBridge>();
    DCHECK(bridge_);
  }
}

void ArcDataSnapshotdManager::OnDaemonStopped(bool success) {
  if (!success) {
    DLOG(ERROR) << "Failed to stop arc-data-snapshotd";
  } else {
    VLOG(1) << "arc-data-snapshotd stopped";
    bridge_.reset();
  }
}

}  // namespace data_snapshotd
}  // namespace arc
