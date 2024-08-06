// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_CYCLE_DATA_TYPE_TRACKER_H_
#define COMPONENTS_SYNC_ENGINE_CYCLE_DATA_TYPE_TRACKER_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/time/time.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/sync_invalidation.h"
#include "components/sync/engine/cycle/commit_quota.h"

namespace sync_pb {
class GetUpdateTriggers;
}  // namespace sync_pb

namespace syncer {

struct WaitInterval {
  enum class BlockingMode {
    // Uninitialized state, should not be set in practice.
    kUnknown = -1,
    // We enter a series of increasingly longer WaitIntervals if we experience
    // repeated transient failures.  We retry at the end of each interval.
    kExponentialBackoff,
    // A server-initiated throttled interval.  We do not allow any syncing
    // during such an interval.
    kThrottled,
    // We re retrying for exponetial backoff.
    kExponentialBackoffRetrying,
  };
  WaitInterval(BlockingMode mode, base::TimeDelta length);
  ~WaitInterval();

  BlockingMode mode;
  base::TimeDelta length;
};

// A class to track the per-type scheduling data.
class DataTypeTracker {
 public:
  explicit DataTypeTracker(DataType type);

  DataTypeTracker(const DataTypeTracker&) = delete;
  DataTypeTracker& operator=(const DataTypeTracker&) = delete;

  ~DataTypeTracker();

  // For STL compatibility, we do not forbid the creation of a default copy
  // constructor and assignment operator.

  // Tracks that a local change has been made to this type.
  void RecordLocalChange();

  // Tracks that a local refresh request has been made for this type.
  void RecordLocalRefreshRequest();

  // Takes note that initial sync is pending for this type.
  void RecordInitialSyncRequired();

  // Takes note that the conflict happended for this type, need to sync to
  // resolve conflict locally.
  void RecordCommitConflict();

  // Records that a commit message has been sent (note that each commit message
  // may include multiple entities of this data type and each sync cycle may
  // include an arbitrary number of commit messages).
  void RecordSuccessfulCommitMessage();

  // Records that a sync cycle has been performed successfully.
  // Generally, this means that all local changes have been committed and all
  // remote changes have been downloaded, so we can clear any flags related to
  // pending work.
  // But if partial throttling and backoff happen, this function also will be
  // called since we count those cases as success. So we need to check if the
  // datatype is in partial throttling or backoff in the beginning of this
  // function.
  void RecordSuccessfulSyncCycleIfNotBlocked();

  // Records that the initial sync has completed successfully. This gets called
  // when the initial configuration/download cycle has finished for this type.
  void RecordInitialSyncDone();

  // Returns true if there is a good reason to perform a sync cycle.  This does
  // not take into account whether or not now is a good time to perform a sync
  // cycle.  That's for the scheduler to decide.
  bool IsSyncRequired() const;

  // Returns true if there is a good reason to fetch updates for this type as
  // part of the next sync cycle.
  bool IsGetUpdatesRequired() const;

  // Returns true if there is an uncommitted local change.
  bool HasLocalChangePending() const;

  // Returns true if we've received an invalidation since we last fetched
  // updates.
  bool HasPendingInvalidation() const;

  // Returns true if an explicit refresh request is still outstanding.
  bool HasRefreshRequestPending() const;

  // Returns true if this type is requesting an initial sync.
  bool IsInitialSyncRequired() const;

  // Returns true if this type is requesting a sync to resolve conflict issue.
  bool IsSyncRequiredToResolveConflict() const;

  // Fills some type-specific contents of a GetUpdates request protobuf.  These
  // messages provide the server with the information it needs to decide how to
  // handle a request.
  void FillGetUpdatesTriggersMessage(sync_pb::GetUpdateTriggers* msg);

  // Returns true if the type is currently throttled or backed off.
  bool IsBlocked() const;

  // Returns the time until this type's throttling or backoff interval expires.
  // Should not be called unless IsThrottled() or IsBackedOff() returns true.
  // The returned value will be increased to zero if it would otherwise have
  // been negative.
  base::TimeDelta GetTimeUntilUnblock() const;

  // Returns the last backoff interval.
  base::TimeDelta GetLastBackoffInterval() const;

  // Throttles the type from |now| until |now| + |duration|.
  void ThrottleType(base::TimeDelta duration, base::TimeTicks now);

  // Backs off the type from |now| until |now| + |duration|.
  void BackOffType(base::TimeDelta duration, base::TimeTicks now);

  // Unblocks the type if base::TimeTicks::Now() >= |unblock_time_| expiry time.
  void UpdateThrottleOrBackoffState();

  // Update |has_pending_invalidations_| flag.
  void SetHasPendingInvalidations(bool has_pending_invalidations);

  // Update the local change nudge delay for this type.
  // No update happens if |delay| is too small (less than the smallest default
  // delay).
  void UpdateLocalChangeNudgeDelay(base::TimeDelta delay);

  // Returns the current local change nudge delay for this type.
  base::TimeDelta GetLocalChangeNudgeDelay(bool is_single_client) const;

  // Returns the current nudge delay for receiving remote invalitation for this
  // type;
  base::TimeDelta GetRemoteInvalidationDelay() const;

  // Return the BlockingMode for this type.
  WaitInterval::BlockingMode GetBlockingMode() const;

  // UpdateLocalChangeNudgeDelay() usually rejects a delay update if the value
  // is too small. This method ignores that check.
  void SetLocalChangeNudgeDelayIgnoringMinForTest(base::TimeDelta delay);

  // Updates the parameters for the commit quota if the data type can receive
  // commits via extension APIs. Empty optional means using the defaults.
  void SetQuotaParamsIfExtensionType(
      std::optional<int> max_tokens,
      std::optional<base::TimeDelta> refill_interval,
      std::optional<base::TimeDelta> depleted_quota_nudge_delay);

 private:
  friend class SyncSchedulerImplTest;

  const DataType type_;

  // Number of local change nudges received for this type since the last
  // successful sync cycle.
  int local_nudge_count_ = 0;

  // Number of local refresh requests received for this type since the last
  // successful sync cycle.
  int local_refresh_request_count_ = 0;

  // Set to true if this type is ready for, but has not yet completed initial
  // sync.
  bool initial_sync_required_ = false;

  // Set to true if this type need to get update to resolve conflict issue.
  bool sync_required_to_resolve_conflict_ = false;

  // Set to true if this type has invalidations that are needed to be used in
  // GetUpdate() trigger message.
  bool has_pending_invalidations_ = false;

  // If !unblock_time_.is_null(), this type is throttled or backed off, check
  // |wait_interval_->mode| for specific reason. Now the datatype may not
  // download or commit data until the specified time.
  base::TimeTicks unblock_time_;

  // Current wait state.  Null if we're not in backoff or throttling.
  std::unique_ptr<WaitInterval> wait_interval_;

  // The amount of time to delay a sync cycle by when a local change for this
  // type occurs.
  base::TimeDelta local_change_nudge_delay_;

  // Quota for commits (used only for data types that can be committed by
  // extensions).
  const std::unique_ptr<CommitQuota> quota_;

  // The amount of time to delay a sync cycle by when a local change for this
  // type occurs and the commit quota is depleted.
  base::TimeDelta depleted_quota_nudge_delay_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_CYCLE_DATA_TYPE_TRACKER_H_
