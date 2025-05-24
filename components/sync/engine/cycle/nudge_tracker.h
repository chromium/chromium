// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_CYCLE_NUDGE_TRACKER_H_
#define COMPONENTS_SYNC_ENGINE_CYCLE_NUDGE_TRACKER_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <optional>

#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/sync_invalidation.h"
#include "components/sync/engine/cycle/data_type_tracker.h"

namespace sync_pb {
class GetUpdateTriggers;
enum SyncEnums_GetUpdatesOrigin : int;
}  // namespace sync_pb

namespace syncer {

// A class to track the outstanding work required to bring the client back into
// sync with the server.
class NudgeTracker {
 public:
  NudgeTracker();

  NudgeTracker(const NudgeTracker&) = delete;
  NudgeTracker& operator=(const NudgeTracker&) = delete;

  ~NudgeTracker();

  // Returns true if there is a good reason for performing a sync cycle.
  // This does not take into account whether or not this is a good *time* to
  // perform a sync cycle; that's the scheduler's job.
  bool IsSyncRequired(DataTypeSet types) const;

  // Returns true if there is a good reason for performing a get updates
  // request as part of the next sync cycle.
  bool IsGetUpdatesRequired(DataTypeSet types) const;

  // Tells this class that a commit message has been sent (note that each sync
  // cycle may include an arbitrary number of commit messages).
  void RecordSuccessfulCommitMessage(DataTypeSet types);

  // Tells this class that all required update fetching or committing has
  // completed successfully, as the result of a "normal" sync cycle.
  // Any blocked data types will ignore this, but non-blocked types and the
  // overall state will still get updated.
  void RecordSuccessfulSyncCycleIfNotBlocked(DataTypeSet types);

  // Tells this class that the initial sync has happened for the given `types`,
  // generally due to a "configuration" cycle.
  void RecordInitialSyncDone(DataTypeSet types);

  // Takes note of a local change.
  // Returns the current nudge delay for local changes to `type`.
  base::TimeDelta RecordLocalChange(DataType type, bool is_single_client);

  // Takes note of a locally issued request to refresh a data type.
  // Returns the nudge delay for a local refresh.
  base::TimeDelta RecordLocalRefreshRequest(DataTypeSet types);

  // Takes note of the receipt of an invalidation notice from the server.
  // Returns the nudge delay for a remote invalidation.
  base::TimeDelta GetRemoteInvalidationDelay(DataType type) const;

  // Take note that an initial sync is pending for this type.
  void RecordInitialSyncRequired(DataType type);

  // Takes note that the conflict happended for this type, need to sync to
  // resolve conflict locally.
  void RecordCommitConflict(DataType type);

  // These functions should be called to keep this class informed of the status
  // of the connection to the invalidations server.
  void OnInvalidationsEnabled();
  void OnInvalidationsDisabled();

  // Marks `types` as being throttled from `now` until `now` + `length`.
  void SetTypesThrottledUntil(DataTypeSet types,
                              base::TimeDelta length,
                              base::TimeTicks now);

  // Marks `type` as being backed off from `now` until `now` + `length`.
  void SetTypeBackedOff(DataType type,
                        base::TimeDelta length,
                        base::TimeTicks now);

  // Removes any throttling and backoff that have expired.
  void UpdateTypeThrottlingAndBackoffState();

  void SetHasPendingInvalidations(DataType type,
                                  bool has_pending_invalidations);

  // Returns the time of the next type unthrottling or unbackoff.
  base::TimeDelta GetTimeUntilNextUnblock() const;

  // Returns the time of for type last backing off interval.
  base::TimeDelta GetTypeLastBackoffInterval(DataType type) const;

  // Returns true if any type is currenlty throttled or backed off.
  bool IsAnyTypeBlocked() const;

  // Returns true if `type` is currently blocked.
  bool IsTypeBlocked(DataType type) const;

  // Returns `type`'s blocking mode.
  WaitInterval::BlockingMode GetTypeBlockingMode(DataType type) const;

  // Returns the set of currently throttled or backed off types.
  DataTypeSet GetBlockedTypes() const;

  // Returns the set of types with local changes pending.
  DataTypeSet GetNudgedTypes() const;

  // Returns the set of types that have pending invalidations.
  DataTypeSet GetNotifiedTypes() const;

  // Returns the set of types that have pending refresh requests.
  DataTypeSet GetRefreshRequestedTypes() const;

  // Returns the 'origin' of the GetUpdate request.
  sync_pb::SyncEnums_GetUpdatesOrigin GetOrigin() const;

  // Fills a GetUpdatesTrigger message for the next GetUpdates request.  This is
  // used by the DownloadUpdatesCommand to dump lots of useful per-type state
  // information into the GetUpdate request before sending it off to the server.
  void FillProtoMessage(DataType type, sync_pb::GetUpdateTriggers* msg) const;

  // Update the per-datatype local change nudge delay. No update happens
  // if `delay` is too small (less than the smallest default delay).
  void UpdateLocalChangeDelay(DataType type, const base::TimeDelta& delay);

  // UpdateLocalChangeDelay() usually rejects a delay update if the value
  // is too small. This method ignores that check.
  void SetLocalChangeDelayIgnoringMinForTest(DataType type,
                                             const base::TimeDelta& delay);

  // Updates the parameters for commit quotas for the data types that can
  // receive commits via extension APIs. Empty optional means using the
  // defaults.
  void SetQuotaParamsForExtensionTypes(
      std::optional<int> max_tokens,
      std::optional<base::TimeDelta> refill_interval,
      std::optional<base::TimeDelta> depleted_quota_nudge_delay);

 private:
  using TypeTrackerMap = std::map<DataType, std::unique_ptr<DataTypeTracker>>;

  friend class SyncSchedulerImplTest;

  TypeTrackerMap type_trackers_;

  // Tracks whether or not invalidations are currently enabled.
  bool invalidations_enabled_ = false;

  // This flag is set if suspect that some technical malfunction or known bug
  // may have left us with some unserviced invalidations.
  //
  // Keeps track of whether or not we're fully in sync with the invalidation
  // server.  This can be false even if invalidations are enabled and working
  // correctly.  For example, until we get ack-tracking working properly, we
  // won't persist invalidations between restarts, so we may be out of sync when
  // we restart.  The only way to get back into sync is to have invalidations
  // enabled, then complete a sync cycle to make sure we're fully up to date.
  bool invalidations_out_of_sync_ = true;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_CYCLE_NUDGE_TRACKER_H_
