// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_CYCLE_STATUS_CONTROLLER_H_
#define COMPONENTS_SYNC_ENGINE_CYCLE_STATUS_CONTROLLER_H_

#include "base/time/time.h"
#include "components/sync/engine/cycle/model_neutral_state.h"

namespace syncer {

// StatusController handles all counter and status related number crunching and
// state tracking on behalf of a SyncCycle.
//
// This object may be accessed from many different threads.  It will be accessed
// most often from the syncer thread.  However, when update application is in
// progress it may also be accessed from the worker threads.  This is safe
// because only one of them will run at a time, and the syncer thread will be
// blocked until update application completes.
//
// This object contains only global state.  None of its members are per model
// type counters.
class StatusController {
 public:
  StatusController();

  StatusController(const StatusController&) = delete;
  StatusController& operator=(const StatusController&) = delete;

  ~StatusController();

  // The types which had non-deletion updates in the GetUpdates during the
  // last sync cycle.
  DataTypeSet get_updated_types() const;
  void add_updated_type(DataType type);
  void clear_updated_types();

  // Various conflict counters.
  int num_server_conflicts() const;

  // Aggregate sum of all conflicting items over all conflict types.
  int TotalNumConflictingItems() const;

  // The time at which we started the most recent sync cycle.
  base::Time sync_start_time() const { return sync_start_time_; }

  // If a poll was performed in this cycle, the time at which it finished.
  // Not set if no poll was performed.
  base::Time poll_finish_time() const { return poll_finish_time_; }

  const ModelNeutralState& model_neutral_state() const {
    return model_neutral_;
  }

  bool last_get_key_failed() const;

  // Download counters.
  void increment_num_updates_downloaded_by(int value);
  void increment_num_tombstone_updates_downloaded_by(int value);

  // Update application and conflict resolution counters.
  void increment_num_server_conflicts();

  // Commit counters.
  void increment_num_successful_commits();
  void increment_num_successful_bookmark_commits();

  // Server communication status tracking.
  void set_last_get_key_failed(bool failed);
  void set_last_download_updates_result(const SyncerError result);
  void set_commit_result(const SyncerError result);

  void UpdateStartTime();
  void UpdatePollTime();

 private:
  ModelNeutralState model_neutral_;

  // Time the last sync cycle began.
  base::Time sync_start_time_;

  // If a poll was performed, the time it finished. Not set if not poll was
  // performed.
  base::Time poll_finish_time_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_CYCLE_STATUS_CONTROLLER_H_
