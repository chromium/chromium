// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/cycle/data_type_tracker.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/notreached.h"

namespace syncer {

namespace {

#define ENUM_CASE(x) \
  case x:            \
    return #x;       \
    break;

}  // namespace

WaitInterval::WaitInterval() : mode(UNKNOWN) {}

WaitInterval::WaitInterval(BlockingMode mode, base::TimeDelta length)
    : mode(mode), length(length) {}

WaitInterval::~WaitInterval() {}

const char* WaitInterval::GetModeString(BlockingMode mode) {
  switch (mode) {
    ENUM_CASE(UNKNOWN);
    ENUM_CASE(EXPONENTIAL_BACKOFF);
    ENUM_CASE(THROTTLED);
    ENUM_CASE(EXPONENTIAL_BACKOFF_RETRYING);
  }
  NOTREACHED();
  return "";
}

#undef ENUM_CASE

DataTypeTracker::DataTypeTracker(size_t initial_payload_buffer_size)
    : local_nudge_count_(0),
      local_refresh_request_count_(0),
      payload_buffer_size_(initial_payload_buffer_size),
      initial_sync_required_(false),
      sync_required_to_resolve_conflict_(false) {}

DataTypeTracker::~DataTypeTracker() {}

base::TimeDelta DataTypeTracker::RecordLocalChange() {
  local_nudge_count_++;
  return nudge_delay_;
}

void DataTypeTracker::RecordLocalRefreshRequest() {
  local_refresh_request_count_++;
}

void DataTypeTracker::RecordRemoteInvalidation(
    std::unique_ptr<InvalidationInterface> incoming) {
  DCHECK(incoming);

  // Merge the incoming invalidation into our list of pending invalidations.
  //
  // We won't use STL algorithms here because our concept of equality doesn't
  // quite fit the expectations of set_intersection.  In particular, two
  // invalidations can be equal according to the SingleObjectInvalidationSet's
  // rules (ie. have equal versions), but still have different AckHandle values
  // and need to be acknowledged separately.
  //
  // The invalidations service can only track one outsanding invalidation per
  // type and version, so the acknowledgement here should be redundant.  We'll
  // acknowledge them anyway since it should do no harm, and makes this code a
  // bit easier to test.
  //
  // Overlaps should be extremely rare for most invalidations.  They can happen
  // for unknown version invalidations, though.

  auto it = pending_invalidations_.begin();

  // Find the lower bound.
  while (it != pending_invalidations_.end() &&
         InvalidationInterface::LessThanByVersion(**it, *incoming)) {
    it++;
  }

  if (it != pending_invalidations_.end() &&
      !InvalidationInterface::LessThanByVersion(*incoming, **it) &&
      !InvalidationInterface::LessThanByVersion(**it, *incoming)) {
    // Incoming overlaps with existing.  Either both are unknown versions
    // (likely) or these two have the same version number (very unlikely).
    // Acknowledge and overwrite existing.

    // Insert before the existing and get iterator to inserted.
    auto it2 = pending_invalidations_.insert(it, std::move(incoming));

    // Increment that iterator to the old one, then acknowledge and remove it.
    ++it2;
    (*it2)->Acknowledge();
    pending_invalidations_.erase(it2);
  } else {
    // The incoming has a version not in the pending_invalidations_ list.
    // Add it to the list at the proper position.
    pending_invalidations_.insert(it, std::move(incoming));
  }

  // The incoming invalidation may have caused us to exceed our buffer size.
  // Trim some items from our list, if necessary.
  while (pending_invalidations_.size() > payload_buffer_size_) {
    last_dropped_invalidation_ = std::move(pending_invalidations_.front());
    last_dropped_invalidation_->Drop();
    pending_invalidations_.erase(pending_invalidations_.begin());
  }
}

void DataTypeTracker::RecordInitialSyncRequired() {
  initial_sync_required_ = true;
}

void DataTypeTracker::RecordCommitConflict() {
  sync_required_to_resolve_conflict_ = true;
}

void DataTypeTracker::RecordSuccessfulSyncCycle() {
  // If we were blocked, then we would have been excluded from this cycle's
  // GetUpdates and Commit actions.  Our state remains unchanged.
  if (IsBlocked()) {
    return;
  }

  // Reset throttling and backoff state.
  unblock_time_ = base::TimeTicks();
  wait_interval_.reset();

  local_nudge_count_ = 0;
  local_refresh_request_count_ = 0;

  // TODO(rlarocque): If we want this to be correct even if we should happen to
  // crash before writing all our state, we should wait until the results of
  // this sync cycle have been written to disk before updating the invalidations
  // state.  See crbug.com/324996.
  for (auto it = pending_invalidations_.begin();
       it != pending_invalidations_.end(); ++it) {
    (*it)->Acknowledge();
  }
  pending_invalidations_.clear();

  if (last_dropped_invalidation_) {
    last_dropped_invalidation_->Acknowledge();
    last_dropped_invalidation_.reset();
  }

  // The initial sync should generally have happened as part of a "configure"
  // sync cycle, before this method gets called (i.e. after a successful
  // "normal" sync cycle). However, in some cases the initial sync might not
  // have happened, e.g. if this one data type got blocked or throttled during
  // the configure cycle. For those cases, also clear |initial_sync_required_|
  // here.
  initial_sync_required_ = false;

  sync_required_to_resolve_conflict_ = false;
}

void DataTypeTracker::RecordInitialSyncDone() {
  // If we were blocked during the initial sync cycle, then the initial sync is
  // not actually done. Our state remains unchanged.
  if (IsBlocked()) {
    return;
  }
  initial_sync_required_ = false;
}

// This limit will take effect on all future invalidations received.
void DataTypeTracker::UpdatePayloadBufferSize(size_t new_size) {
  payload_buffer_size_ = new_size;
}

bool DataTypeTracker::IsSyncRequired() const {
  return !IsBlocked() && (HasLocalChangePending() || IsGetUpdatesRequired());
}

bool DataTypeTracker::IsGetUpdatesRequired() const {
  // TODO(crbug.com/926184): Maybe this shouldn't check IsInitialSyncRequired():
  // The initial sync is done in a configuration cycle, while this method
  // refers to normal cycles.
  return !IsBlocked() &&
         (HasRefreshRequestPending() || HasPendingInvalidation() ||
          IsInitialSyncRequired() || IsSyncRequiredToResolveConflict());
}

bool DataTypeTracker::HasLocalChangePending() const {
  return local_nudge_count_ > 0;
}

bool DataTypeTracker::HasRefreshRequestPending() const {
  return local_refresh_request_count_ > 0;
}

bool DataTypeTracker::HasPendingInvalidation() const {
  return !pending_invalidations_.empty() || last_dropped_invalidation_;
}

bool DataTypeTracker::IsInitialSyncRequired() const {
  return initial_sync_required_;
}

bool DataTypeTracker::IsSyncRequiredToResolveConflict() const {
  return sync_required_to_resolve_conflict_;
}

void DataTypeTracker::SetLegacyNotificationHint(
    sync_pb::DataTypeProgressMarker* progress) const {
  DCHECK(!IsBlocked())
      << "We should not make requests if the type is throttled or backed off.";

  if (!pending_invalidations_.empty() &&
      !pending_invalidations_.back()->IsUnknownVersion()) {
    // The old-style source info can contain only one hint per type.  We grab
    // the most recent, to mimic the old coalescing behaviour.
    progress->set_notification_hint(
        pending_invalidations_.back()->GetPayload());
  } else if (HasLocalChangePending()) {
    // The old-style source info sent up an empty string (as opposed to
    // nothing at all) when the type was locally nudged, but had not received
    // any invalidations.
    progress->set_notification_hint(std::string());
  }
}

void DataTypeTracker::FillGetUpdatesTriggersMessage(
    sync_pb::GetUpdateTriggers* msg) const {
  // Fill the list of payloads, if applicable.  The payloads must be ordered
  // oldest to newest, so we insert them in the same order as we've been storing
  // them internally.
  for (auto it = pending_invalidations_.begin();
       it != pending_invalidations_.end(); ++it) {
    if (!(*it)->IsUnknownVersion()) {
      msg->add_notification_hint((*it)->GetPayload());
    }
  }

  msg->set_server_dropped_hints(
      !pending_invalidations_.empty() &&
      (*pending_invalidations_.begin())->IsUnknownVersion());
  msg->set_client_dropped_hints(!!last_dropped_invalidation_);
  msg->set_local_modification_nudges(local_nudge_count_);
  msg->set_datatype_refresh_nudges(local_refresh_request_count_);
  msg->set_initial_sync_in_progress(initial_sync_required_);
  msg->set_sync_for_resolve_conflict_in_progress(
      sync_required_to_resolve_conflict_);
}

bool DataTypeTracker::IsBlocked() const {
  return wait_interval_.get() &&
         (wait_interval_->mode == WaitInterval::THROTTLED ||
          wait_interval_->mode == WaitInterval::EXPONENTIAL_BACKOFF);
}

base::TimeDelta DataTypeTracker::GetTimeUntilUnblock() const {
  DCHECK(IsBlocked());
  return std::max(base::TimeDelta::FromSeconds(0),
                  unblock_time_ - base::TimeTicks::Now());
}

base::TimeDelta DataTypeTracker::GetLastBackoffInterval() const {
  if (GetBlockingMode() != WaitInterval::EXPONENTIAL_BACKOFF_RETRYING) {
    NOTREACHED();
    return base::TimeDelta::FromSeconds(0);
  }
  return wait_interval_->length;
}

void DataTypeTracker::ThrottleType(base::TimeDelta duration,
                                   base::TimeTicks now) {
  unblock_time_ = std::max(unblock_time_, now + duration);
  wait_interval_ =
      std::make_unique<WaitInterval>(WaitInterval::THROTTLED, duration);
}

void DataTypeTracker::BackOffType(base::TimeDelta duration,
                                  base::TimeTicks now) {
  unblock_time_ = std::max(unblock_time_, now + duration);
  wait_interval_ = std::make_unique<WaitInterval>(
      WaitInterval::EXPONENTIAL_BACKOFF, duration);
}

void DataTypeTracker::UpdateThrottleOrBackoffState() {
  if (base::TimeTicks::Now() >= unblock_time_) {
    if (wait_interval_.get() &&
        (wait_interval_->mode == WaitInterval::EXPONENTIAL_BACKOFF ||
         wait_interval_->mode == WaitInterval::EXPONENTIAL_BACKOFF_RETRYING)) {
      wait_interval_->mode = WaitInterval::EXPONENTIAL_BACKOFF_RETRYING;
    } else {
      unblock_time_ = base::TimeTicks();
      wait_interval_.reset();
    }
  }
}

void DataTypeTracker::UpdateLocalNudgeDelay(base::TimeDelta delay) {
  nudge_delay_ = delay;
}

WaitInterval::BlockingMode DataTypeTracker::GetBlockingMode() const {
  if (!wait_interval_) {
    return WaitInterval::UNKNOWN;
  }
  return wait_interval_->mode;
}

}  // namespace syncer
