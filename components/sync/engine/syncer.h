// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_SYNCER_H_
#define COMPONENTS_SYNC_ENGINE_SYNCER_H_

#include <stdint.h>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "components/sync/base/data_type.h"
#include "components/sync/engine/syncer_error.h"

namespace sync_pb {
enum SyncEnums_GetUpdatesOrigin : int;
}  // namespace sync_pb

namespace syncer {

class CancelationSignal;
class GetUpdatesDelegate;
class NudgeTracker;
class SyncCycle;

// This enum should be in sync with SyncerErrorValues in enums.xml. These
// values are persisted to logs. Entries should not be renumbered and numeric
// values should never be reused. Exposed for tests.
// TODO(crbug.com/40864723): this enum no longer corresponds to SyncerError,
// modernize it.
// LINT.IfChange(SyncerErrorValues)
enum class SyncerErrorValueForUma {
  // Deprecated: kUnset = 0,  // Default value.
  // Deprecated: CANNOT_DO_WORK = 1,

  kNetworkConnectionUnavailable = 2,  // Connectivity failure.
  // Deprecated: NETWORK_IO_ERROR = 3,
  kSyncServerError = 4,  // Non auth HTTP error.
  kSyncAuthError = 5,    // HTTP auth error.

  // Based on values returned by server.  Most are defined in sync.proto.
  // Deprecated: SERVER_RETURN_INVALID_CREDENTIAL = 6,
  kServerReturnUnknownError = 7,
  kServerReturnThrottled = 8,
  kServerReturnTransientError = 9,
  kServerReturnMigrationDone = 10,
  // Deprecated: kServerReturnClearPending = 11,
  kServerReturnNotMyBirthday = 12,
  kServerReturnConflict = 13,
  kServerResponseValidationFailed = 14,
  kServerReturnDisabledByAdmin = 15,
  // Deprecated: SERVER_RETURN_USER_ROLLBACK = 16,
  // Deprecated: SERVER_RETURN_PARTIAL_FAILURE = 17,
  kServerReturnClientDataObsolete = 18,
  kServerReturnEncryptionObsolete = 19,

  // Deprecated: DATATYPE_TRIGGERED_RETRY = 20,
  // Deprecated: SERVER_MORE_TO_DOWNLOAD = 21,

  kSyncerOk = 22,

  kMaxValue = kSyncerOk,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sync/enums.xml:SyncerErrorValues)

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

  Syncer(const Syncer&) = delete;
  Syncer& operator=(const Syncer&) = delete;

  virtual ~Syncer();

  // Whether the syncer is in the middle of a sync cycle.
  bool IsSyncing() const;

  // Fetches and applies updates, resolves conflicts and commits local changes
  // for |request_types| as necessary until client and server states are in
  // sync.  The |nudge_tracker| contains state that describes why the client is
  // out of sync and what must be done to bring it back into sync.
  // Returns: false if an error occurred and retries should backoff, true
  // otherwise.
  virtual bool NormalSyncShare(DataTypeSet request_types,
                               NudgeTracker* nudge_tracker,
                               SyncCycle* cycle);

  // Performs an initial download for the |request_types|.  It is assumed that
  // the specified types have no local state, so none of the downloaded updates
  // will be applied to the model.  The |source| is sent up to the server for
  // debug purposes.  It describes the reason for performing this initial
  // download.
  // Returns: false if an error occurred and retries should backoff, true
  // otherwise.
  virtual bool ConfigureSyncShare(const DataTypeSet& request_types,
                                  sync_pb::SyncEnums_GetUpdatesOrigin origin,
                                  SyncCycle* cycle);

  // Requests to download updates for the |request_types|.  For a well-behaved
  // client with a working connection to the invalidations server, this should
  // be unnecessary.  It may be invoked periodically to try to keep the client
  // in sync despite bugs or transient failures.
  // Returns: false if an error occurred and retries should backoff, true
  // otherwise.
  virtual bool PollSyncShare(DataTypeSet request_types, SyncCycle* cycle);

 private:
  bool DownloadAndApplyUpdates(DataTypeSet* request_types,
                               SyncCycle* cycle,
                               const GetUpdatesDelegate& delegate);

  // This function will commit batches of unsynced items to the server until the
  // number of unsynced and ready to commit items reaches zero or an error is
  // encountered.  A request to exit early will be treated as an error and will
  // abort any blocking operations.
  SyncerError BuildAndPostCommits(const DataTypeSet& request_types,
                                  NudgeTracker* nudge_tracker,
                                  SyncCycle* cycle);

  // Whether an early exist was requested due to a cancelation signal.
  bool ExitRequested();

  bool HandleCycleEnd(SyncCycle* cycle,
                      sync_pb::SyncEnums_GetUpdatesOrigin origin);

  const raw_ptr<CancelationSignal> cancelation_signal_;

  // Whether the syncer is in the middle of a sync attempt.
  bool is_syncing_ = false;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_SYNCER_H_
