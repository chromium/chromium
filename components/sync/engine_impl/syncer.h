// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_SYNCER_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_SYNCER_H_

#include <stdint.h>

#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/syncer_error.h"
#include "components/sync/protocol/sync.pb.h"

namespace syncer {

class CancelationSignal;
class GetUpdatesDelegate;
class NudgeTracker;
class SyncCycle;

// A Syncer provides a control interface for driving the sync cycle.  These
// cycles consist of downloading updates, parsing the response (aka. process
// updates), applying updates while resolving conflicts, and committing local
// changes.  Some of these steps may be skipped if they're deemed to be
// unnecessary.
//
// A Syncer instance expects to run on a dedicated thread.  Calls to SyncShare()
// may take an unbounded amount of time because it may block on network I/O, on
// lock contention, or on tasks posted to other threads.
class Syncer {
 public:
  explicit Syncer(CancelationSignal* cancelation_signal);
  virtual ~Syncer();

  // Whether the syncer is in the middle of a sync cycle.
  bool IsSyncing() const;

  // Fetches and applies updates, resolves conflicts and commits local changes
  // for |request_types| as necessary until client and server states are in
  // sync.  The |nudge_tracker| contains state that describes why the client is
  // out of sync and what must be done to bring it back into sync.
  // Returns: false if an error occurred and retries should backoff, true
  // otherwise.
  virtual bool NormalSyncShare(ModelTypeSet request_types,
                               NudgeTracker* nudge_tracker,
                               SyncCycle* cycle);

  // Performs an initial download for the |request_types|.  It is assumed that
  // the specified types have no local state, and that their associated change
  // processors are in "passive" mode, so none of the downloaded updates will be
  // applied to the model.  The |source| is sent up to the server for debug
  // purposes.  It describes the reson for performing this initial download.
  // Returns: false if an error occurred and retries should backoff, true
  // otherwise.
  virtual bool ConfigureSyncShare(const ModelTypeSet& request_types,
                                  sync_pb::SyncEnums::GetUpdatesOrigin origin,
                                  SyncCycle* cycle);

  // Requests to download updates for the |request_types|.  For a well-behaved
  // client with a working connection to the invalidations server, this should
  // be unnecessary.  It may be invoked periodically to try to keep the client
  // in sync despite bugs or transient failures.
  // Returns: false if an error occurred and retries should backoff, true
  // otherwise.
  virtual bool PollSyncShare(ModelTypeSet request_types, SyncCycle* cycle);

 private:
  bool DownloadAndApplyUpdates(ModelTypeSet* request_types,
                               SyncCycle* cycle,
                               const GetUpdatesDelegate& delegate);

  // This function will commit batches of unsynced items to the server until the
  // number of unsynced and ready to commit items reaches zero or an error is
  // encountered.  A request to exit early will be treated as an error and will
  // abort any blocking operations.
  SyncerError BuildAndPostCommits(const ModelTypeSet& request_types,
                                  NudgeTracker* nudge_tracker,
                                  SyncCycle* cycle);

  // Whether an early exist was requested due to a cancelation signal.
  bool ExitRequested();

  bool HandleCycleEnd(SyncCycle* cycle,
                      sync_pb::SyncEnums::GetUpdatesOrigin origin);

  CancelationSignal* const cancelation_signal_;

  // Whether the syncer is in the middle of a sync attempt.
  bool is_syncing_;

  DISALLOW_COPY_AND_ASSIGN(Syncer);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_SYNCER_H_
