// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_STARTUP_CONTROLLER_H_
#define COMPONENTS_SYNC_DRIVER_STARTUP_CONTROLLER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/sync/base/model_type.h"

namespace syncer {

// This class is used by ProfileSyncService to manage all logic and state
// pertaining to initialization of the SyncEngine.
class StartupController {
 public:
  enum class State {
    // Startup has not been triggered yet.
    NOT_STARTED,
    // Startup has been triggered but is deferred. The actual startup will
    // happen once the deferred delay expires (or when immediate startup is
    // requested, whichever happens first).
    STARTING_DEFERRED,
    // Startup has happened, i.e. |start_engine| has been run.
    STARTED
  };

  StartupController(
      base::RepeatingCallback<ModelTypeSet()> get_preferred_data_types,
      base::RepeatingCallback<bool()> should_start,
      base::RepeatingClosure start_engine);
  ~StartupController();

  // Starts up sync if it is requested by the user and preconditions are met.
  // If |force_immediate| is true, this will start sync immediately, bypassing
  // deferred startup and the "first setup complete" check (but *not* the
  // |should_start_callback_| check!).
  void TryStart(bool force_immediate);

  // Called when a datatype (SyncableService) has a need for sync to start
  // ASAP, presumably because a local change event has occurred but we're
  // still in deferred start mode, meaning the SyncableService hasn't been
  // told to MergeDataAndStartSyncing yet.
  // It is expected that |type| is a currently active datatype.
  void OnDataTypeRequestsSyncStartup(ModelType type);

  // Prepares this object for a new attempt to start sync, forgetting
  // whether or not preconditions were previously met.
  void Reset();

  State GetState() const;

  base::Time start_engine_time() const { return start_engine_time_; }

 private:
  enum StartUpDeferredOption { STARTUP_DEFERRED, STARTUP_IMMEDIATE };

  void StartUp(StartUpDeferredOption deferred_option);
  void OnFallbackStartupTimerExpired();

  // Records time spent in deferred state with UMA histograms.
  void RecordTimeDeferred();

  const base::RepeatingCallback<ModelTypeSet()>
      get_preferred_data_types_callback_;

  // A function that can be invoked repeatedly to determine whether sync should
  // be started. |start_engine_| should not be invoked unless this returns true.
  const base::RepeatingCallback<bool()> should_start_callback_;

  // The callback we invoke when it's time to call expensive
  // startup routines for the sync engine.
  const base::RepeatingClosure start_engine_callback_;

  // True if we should start sync ASAP because either a data type has requested
  // it or our deferred startup timer has expired.
  bool bypass_deferred_startup_;

  // The time that StartUp() is called. This is used to calculate time spent
  // in the deferred state; that is, after StartUp and before invoking the
  // start_engine_ callback. If this is non-null, then a (possibly deferred)
  // startup has been triggered.
  base::Time start_up_time_;

  // The time at which we invoked the |start_engine_| callback. If this is
  // non-null, then |start_engine_| shouldn't be called again.
  base::Time start_engine_time_;

  base::WeakPtrFactory<StartupController> weak_factory_{this};
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_STARTUP_CONTROLLER_H_
