// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/cycle/nudge_tracker.h"

#include <algorithm>
#include <utility>

#include "components/sync/engine/polling_constants.h"

namespace syncer {

namespace {

// Delays for syncer nudges.
const int kDefaultNudgeDelayMilliseconds = 200;
const int kSlowNudgeDelayMilliseconds = 2000;
const int kSyncRefreshDelayMilliseconds = 500;
const int kSyncSchedulerDelayMilliseconds = 250;

base::TimeDelta GetDefaultDelayForType(ModelType model_type,
                                       base::TimeDelta minimum_delay) {
  switch (model_type) {
    case AUTOFILL:
    case USER_EVENTS:
      // Accompany types rely on nudges from other types, and hence have long
      // nudge delays.
      return base::TimeDelta::FromSeconds(kDefaultPollIntervalSeconds);
    case BOOKMARKS:
    case PREFERENCES:
    case SESSIONS:
    case FAVICON_IMAGES:
    case FAVICON_TRACKING:
      // Types with sometimes automatic changes get longer delays to allow more
      // coalescing.
      return base::TimeDelta::FromMilliseconds(kSlowNudgeDelayMilliseconds);
    default:
      return minimum_delay;
  }
}

}  // namespace

NudgeTracker::NudgeTracker()
    : invalidations_enabled_(false),
      invalidations_out_of_sync_(true),
      minimum_local_nudge_delay_(
          base::TimeDelta::FromMilliseconds(kDefaultNudgeDelayMilliseconds)),
      local_refresh_nudge_delay_(
          base::TimeDelta::FromMilliseconds(kSyncRefreshDelayMilliseconds)),
      remote_invalidation_nudge_delay_(
          base::TimeDelta::FromMilliseconds(kSyncSchedulerDelayMilliseconds)) {
  // Default initialize all the type trackers.
  for (ModelType type : ProtocolTypes()) {
    type_trackers_.emplace(type, std::make_unique<DataTypeTracker>());
  }
}

NudgeTracker::~NudgeTracker() {}

bool NudgeTracker::IsSyncRequired(ModelTypeSet types) const {
  if (IsRetryRequired()) {
    return true;
  }

  for (ModelType type : types) {
    TypeTrackerMap::const_iterator tracker_it = type_trackers_.find(type);
    DCHECK(tracker_it != type_trackers_.end()) << ModelTypeToString(type);
    if (tracker_it->second->IsSyncRequired()) {
      return true;
    }
  }

  return false;
}

bool NudgeTracker::IsGetUpdatesRequired(ModelTypeSet types) const {
  if (invalidations_out_of_sync_) {
    return true;
  }

  if (IsRetryRequired()) {
    return true;
  }

  for (ModelType type : types) {
    TypeTrackerMap::const_iterator tracker_it = type_trackers_.find(type);
    DCHECK(tracker_it != type_trackers_.end()) << ModelTypeToString(type);
    if (tracker_it->second->IsGetUpdatesRequired()) {
      return true;
    }
  }
  return false;
}

bool NudgeTracker::IsRetryRequired() const {
  if (sync_cycle_start_time_.is_null()) {
    return false;
  }

  if (current_retry_time_.is_null()) {
    return false;
  }

  return current_retry_time_ <= sync_cycle_start_time_;
}

void NudgeTracker::RecordSuccessfulSyncCycle(ModelTypeSet types) {
  // If a retry was required, we've just serviced it.  Unset the flag.
  if (IsRetryRequired()) {
    current_retry_time_ = base::TimeTicks();
  }

  // A successful cycle while invalidations are enabled puts us back into sync.
  invalidations_out_of_sync_ = !invalidations_enabled_;

  for (ModelType type : types) {
    TypeTrackerMap::const_iterator tracker_it = type_trackers_.find(type);
    DCHECK(tracker_it != type_trackers_.end()) << ModelTypeToString(type);
    tracker_it->second->RecordSuccessfulSyncCycle();
  }
}

void NudgeTracker::RecordInitialSyncDone(ModelTypeSet types) {
  for (ModelType type : types) {
    TypeTrackerMap::const_iterator tracker_it = type_trackers_.find(type);
    DCHECK(tracker_it != type_trackers_.end()) << ModelTypeToString(type);
    tracker_it->second->RecordInitialSyncDone();
  }
}

base::TimeDelta NudgeTracker::RecordLocalChange(ModelTypeSet types) {
  // Start with the longest delay.
  base::TimeDelta delay =
      base::TimeDelta::FromSeconds(kDefaultPollIntervalSeconds);
  for (ModelType type : types) {
    TypeTrackerMap::const_iterator tracker_it = type_trackers_.find(type);
    DCHECK(tracker_it != type_trackers_.end());

    // Only if the type tracker has a valid delay (non-zero) that is shorter
    // than the calculated delay do we update the calculated delay.
    base::TimeDelta type_delay = tracker_it->second->RecordLocalChange();
    if (type_delay.is_zero()) {
      type_delay = GetDefaultDelayForType(type, minimum_local_nudge_delay_);
    }
    if (type_delay < delay) {
      delay = type_delay;
    }
  }
  return delay;
}

base::TimeDelta NudgeTracker::RecordLocalRefreshRequest(ModelTypeSet types) {
  for (ModelType type : types) {
    TypeTrackerMap::const_iterator tracker_it = type_trackers_.find(type);
    DCHECK(tracker_it != type_trackers_.end()) << ModelTypeToString(type);
    tracker_it->second->RecordLocalRefreshRequest();
  }
  return local_refresh_nudge_delay_;
}

base::TimeDelta NudgeTracker::RecordRemoteInvalidation(
    ModelType type,
    std::unique_ptr<InvalidationInterface> invalidation) {
  // Forward the invalidations to the proper recipient.
  TypeTrackerMap::const_iterator tracker_it = type_trackers_.find(type);
  DCHECK(tracker_it != type_trackers_.end());
  tracker_it->second->RecordRemoteInvalidation(std::move(invalidation));
  return remote_invalidation_nudge_delay_;
}

void NudgeTracker::RecordInitialSyncRequired(ModelType type) {
  TypeTrackerMap::const_iterator tracker_it = type_trackers_.find(type);
  DCHECK(tracker_it != type_trackers_.end());
  tracker_it->second->RecordInitialSyncRequired();
}

void NudgeTracker::RecordCommitConflict(ModelType type) {
  TypeTrackerMap::const_iterator tracker_it = type_trackers_.find(type);
  DCHECK(tracker_it != type_trackers_.end());
  tracker_it->second->RecordCommitConflict();
}

void NudgeTracker::OnInvalidationsEnabled() {
  invalidations_enabled_ = true;
}

void NudgeTracker::OnInvalidationsDisabled() {
  invalidations_enabled_ = false;
  invalidations_out_of_sync_ = true;
}

void NudgeTracker::SetTypesThrottledUntil(ModelTypeSet types,
                                          base::TimeDelta length,
                                          base::TimeTicks now) {
  for (ModelType type : types) {
    TypeTrackerMap::const_iterator tracker_it = type_trackers_.find(type);
    tracker_it->second->ThrottleType(length, now);
  }
}

void NudgeTracker::SetTypeBackedOff(ModelType type,
                                    base::TimeDelta length,
                                    base::TimeTicks now) {
  TypeTrackerMap::const_iterator tracker_it = type_trackers_.find(type);
  DCHECK(tracker_it != type_trackers_.end());
  tracker_it->second->BackOffType(length, now);
}

void NudgeTracker::UpdateTypeThrottlingAndBackoffState() {
  for (const auto& type_and_tracker : type_trackers_) {
    type_and_tracker.second->UpdateThrottleOrBackoffState();
  }
}

bool NudgeTracker::IsAnyTypeBlocked() const {
  for (const auto& type_and_tracker : type_trackers_) {
    if (type_and_tracker.second->IsBlocked()) {
      return true;
    }
  }
  return false;
}

bool NudgeTracker::IsTypeBlocked(ModelType type) const {
  DCHECK(type_trackers_.find(type) != type_trackers_.end());
  return type_trackers_.find(type)->second->IsBlocked();
}

WaitInterval::BlockingMode NudgeTracker::GetTypeBlockingMode(
    ModelType type) const {
  DCHECK(type_trackers_.find(type) != type_trackers_.end());
  return type_trackers_.find(type)->second->GetBlockingMode();
}

base::TimeDelta NudgeTracker::GetTimeUntilNextUnblock() const {
  DCHECK(IsAnyTypeBlocked()) << "This function requires a pending unblock.";

  // Return min of GetTimeUntilUnblock() values for all IsBlocked() types.
  base::TimeDelta time_until_next_unblock = base::TimeDelta::Max();
  for (const auto& type_and_tracker : type_trackers_) {
    if (type_and_tracker.second->IsBlocked()) {
      time_until_next_unblock =
          std::min(time_until_next_unblock,
                   type_and_tracker.second->GetTimeUntilUnblock());
    }
  }
  DCHECK(!time_until_next_unblock.is_max());

  return time_until_next_unblock;
}

base::TimeDelta NudgeTracker::GetTypeLastBackoffInterval(ModelType type) const {
  auto tracker_it = type_trackers_.find(type);
  DCHECK(tracker_it != type_trackers_.end());

  return tracker_it->second->GetLastBackoffInterval();
}

ModelTypeSet NudgeTracker::GetBlockedTypes() const {
  ModelTypeSet result;
  for (const auto& type_and_tracker : type_trackers_) {
    if (type_and_tracker.second->IsBlocked()) {
      result.Put(type_and_tracker.first);
    }
  }
  return result;
}

ModelTypeSet NudgeTracker::GetNudgedTypes() const {
  ModelTypeSet result;
  for (const auto& type_and_tracker : type_trackers_) {
    if (type_and_tracker.second->HasLocalChangePending()) {
      result.Put(type_and_tracker.first);
    }
  }
  return result;
}

ModelTypeSet NudgeTracker::GetNotifiedTypes() const {
  ModelTypeSet result;
  for (const auto& type_and_tracker : type_trackers_) {
    if (type_and_tracker.second->HasPendingInvalidation()) {
      result.Put(type_and_tracker.first);
    }
  }
  return result;
}

ModelTypeSet NudgeTracker::GetRefreshRequestedTypes() const {
  ModelTypeSet result;
  for (const auto& type_and_tracker : type_trackers_) {
    if (type_and_tracker.second->HasRefreshRequestPending()) {
      result.Put(type_and_tracker.first);
    }
  }
  return result;
}

void NudgeTracker::SetLegacyNotificationHint(
    ModelType type,
    sync_pb::DataTypeProgressMarker* progress) const {
  DCHECK(type_trackers_.find(type) != type_trackers_.end());
  type_trackers_.find(type)->second->SetLegacyNotificationHint(progress);
}

sync_pb::SyncEnums::GetUpdatesOrigin NudgeTracker::GetOrigin() const {
  for (const auto& type_and_tracker : type_trackers_) {
    const DataTypeTracker& tracker = *type_and_tracker.second;
    if (!tracker.IsBlocked() &&
        (tracker.HasPendingInvalidation() ||
         tracker.HasRefreshRequestPending() ||
         tracker.HasLocalChangePending() || tracker.IsInitialSyncRequired())) {
      return sync_pb::SyncEnums::GU_TRIGGER;
    }
  }

  if (IsRetryRequired()) {
    return sync_pb::SyncEnums::RETRY;
  }

  return sync_pb::SyncEnums::UNKNOWN_ORIGIN;
}

void NudgeTracker::FillProtoMessage(ModelType type,
                                    sync_pb::GetUpdateTriggers* msg) const {
  DCHECK(type_trackers_.find(type) != type_trackers_.end());

  // Fill what we can from the global data.
  msg->set_invalidations_out_of_sync(invalidations_out_of_sync_);

  // Delegate the type-specific work to the DataTypeTracker class.
  type_trackers_.find(type)->second->FillGetUpdatesTriggersMessage(msg);
}

void NudgeTracker::SetSyncCycleStartTime(base::TimeTicks now) {
  sync_cycle_start_time_ = now;

  // If current_retry_time_ is still set, that means we have an old retry time
  // left over from a previous cycle.  For example, maybe we tried to perform
  // this retry, hit a network connection error, and now we're in exponential
  // backoff.  In that case, we want this sync cycle to include the GU retry
  // flag so we leave this variable set regardless of whether or not there is an
  // overwrite pending.
  if (!current_retry_time_.is_null()) {
    return;
  }

  // If do not have a current_retry_time_, but we do have a next_retry_time_ and
  // it is ready to go, then we set it as the current_retry_time_.  It will stay
  // there until a GU retry has succeeded.
  if (!next_retry_time_.is_null() &&
      next_retry_time_ <= sync_cycle_start_time_) {
    current_retry_time_ = next_retry_time_;
    next_retry_time_ = base::TimeTicks();
  }
}

void NudgeTracker::SetHintBufferSize(size_t size) {
  for (const auto& type_and_tracker : type_trackers_) {
    type_and_tracker.second->UpdatePayloadBufferSize(size);
  }
}

void NudgeTracker::SetNextRetryTime(base::TimeTicks retry_time) {
  next_retry_time_ = retry_time;
}

void NudgeTracker::OnReceivedCustomNudgeDelays(
    const std::map<ModelType, base::TimeDelta>& delay_map) {
  for (const auto& type_and_delay : delay_map) {
    ModelType type = type_and_delay.first;
    base::TimeDelta delay = type_and_delay.second;
    DCHECK(ProtocolTypes().Has(type));
    TypeTrackerMap::const_iterator type_iter = type_trackers_.find(type);
    if (type_iter == type_trackers_.end()) {
      continue;
    }
    DataTypeTracker* type_tracker = type_iter->second.get();

    if (delay > minimum_local_nudge_delay_) {
      type_tracker->UpdateLocalNudgeDelay(delay);
    } else {
      type_tracker->UpdateLocalNudgeDelay(
          GetDefaultDelayForType(type, minimum_local_nudge_delay_));
    }
  }
}

void NudgeTracker::SetDefaultNudgeDelay(base::TimeDelta nudge_delay) {
  minimum_local_nudge_delay_ = nudge_delay;
}

}  // namespace syncer
