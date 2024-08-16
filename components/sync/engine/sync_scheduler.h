// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_SYNC_SCHEDULER_H_
#define COMPONENTS_SYNC_ENGINE_SYNC_SCHEDULER_H_

#include "base/compiler_specific.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "components/sync/base/sync_invalidation.h"
#include "components/sync/engine/cycle/sync_cycle.h"
#include "services/network/public/mojom/network_change_manager.mojom.h"

namespace syncer {

// A class to schedule syncer tasks intelligently.
class SyncScheduler : public SyncCycle::Delegate {
 public:
  enum Mode {
    // In this mode, the thread only performs configuration tasks.  This is
    // designed to make the case where we want to download updates for a
    // specific type only, and not continue syncing until we are moved into
    // normal mode.
    CONFIGURATION_MODE,
    // Resumes polling and allows nudges, drops configuration tasks.  Runs
    // through entire sync cycle.
    NORMAL_MODE,
  };

  // All methods of SyncScheduler must be called on the same thread
  // (except for RequestEarlyExit()).

  SyncScheduler() = default;
  ~SyncScheduler() override = default;

  // Start the scheduler with the given mode.  If the scheduler is already
  // started, switch to the given mode, although some scheduled tasks from the
  // old mode may still run. |last_poll_time| is used to schedule the initial
  // poll timer.
  virtual void Start(Mode mode, base::Time last_poll_time) = 0;

  // Schedules the configuration task. |ready_task| is invoked when the
  // configuration finishes.
  // Note: must already be in CONFIGURATION mode.
  virtual void ScheduleConfiguration(
      sync_pb::SyncEnums::GetUpdatesOrigin origin,
      DataTypeSet types_to_download,
      base::OnceClosure ready_task) = 0;

  // Request that the syncer avoid starting any new tasks and prepare for
  // shutdown.
  virtual void Stop() = 0;

  // The meat and potatoes. All three of the following methods will post a
  // delayed task to attempt the actual nudge (see ScheduleNudgeImpl).
  //
  // NOTE: |desired_delay| is best-effort. If a nudge is already scheduled to
  // depart earlier than Now() + delay, the scheduler can and will prefer to
  // batch the two so that only one nudge is sent (at the earlier time). Also,
  // as always with delayed tasks and timers, it's possible the task gets run
  // any time after |desired_delay|.

  // The LocalNudge indicates that we've made a local change, and that the
  // syncer should plan to commit this to the server some time soon.
  virtual void ScheduleLocalNudge(DataType type) = 0;

  // The LocalRefreshRequest occurs when we decide for some reason to manually
  // request updates.  This should be used sparingly.  For example, one of its
  // uses is to fetch the latest tab sync data when it's relevant to the UI on
  // platforms where tab sync is not registered for invalidations.
  virtual void ScheduleLocalRefreshRequest(DataTypeSet types) = 0;

  // Invalidations are notifications the server sends to let us know when other
  // clients have committed data.  We need to contact the sync server (being
  // careful to pass along the "hints" delivered with those invalidations) in
  // order to fetch the update.
  virtual void ScheduleInvalidationNudge(DataType type) = 0;

  // Requests a non-blocking initial sync request for the specified type.
  //
  // Many types can only complete initial sync while the scheduler is in
  // configure mode, but a few of them are able to perform their initial sync
  // while the scheduler is in normal mode.  This non-blocking initial sync
  // can be requested through this function.
  virtual void ScheduleInitialSyncNudge(DataType data_type) = 0;

  // Change status of notifications in the SyncCycleContext.
  virtual void SetNotificationsEnabled(bool notifications_enabled) = 0;

  // Called when credentials are updated by the user.
  virtual void OnCredentialsUpdated() = 0;

  // Called when the network layer detects a connection status change.
  virtual void OnConnectionStatusChange(
      network::mojom::ConnectionType type) = 0;

  // Update pending invalidations state in DataTypeTracker. Called whenever
  // invalidation comes or drops.
  virtual void SetHasPendingInvalidations(DataType type,
                                          bool has_pending_invalidations) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_SYNC_SCHEDULER_H_
