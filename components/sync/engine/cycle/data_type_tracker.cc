// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/cycle/data_type_tracker.h"

#include <algorithm>
#include <memory>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/engine/polling_constants.h"
#include "components/sync/protocol/data_type_progress_marker.pb.h"

namespace syncer {

namespace {

// Possible nudge delays for local changes.
constexpr base::TimeDelta kMinLocalChangeNudgeDelay = base::Milliseconds(50);
constexpr base::TimeDelta kMediumLocalChangeNudgeDelay =
    base::Milliseconds(200);
constexpr base::TimeDelta kBigLocalChangeNudgeDelay = base::Milliseconds(2000);
constexpr base::TimeDelta kVeryBigLocalChangeNudgeDelay = kDefaultPollInterval;

constexpr base::TimeDelta kDefaultLocalChangeNudgeDelayForSessions =
    base::Seconds(15);

// Nudge delay for remote invalidations. Common to all data types.
constexpr base::TimeDelta kRemoteInvalidationDelay = base::Milliseconds(250);

// Nudge delay for local changes & remote invalidations for extension-related
// types when their quota is depleted.
constexpr base::TimeDelta kDepletedQuotaNudgeDelayForExtensionTypes =
    base::Seconds(100);

constexpr base::TimeDelta kRefillIntervalForExtensionTypes = base::Seconds(100);
constexpr int kInitialTokensForExtensionTypes = 100;

base::TimeDelta GetDefaultLocalChangeNudgeDelay(DataType data_type) {
  switch (data_type) {
    case AUTOFILL:
    case USER_EVENTS:
      // Accompany types rely on nudges from other types, and hence have long
      // nudge delays.
      return kVeryBigLocalChangeNudgeDelay;
    case SESSIONS:
    case HISTORY:
    case COOKIES:
      // Sessions is the type that causes the most commit traffic. It gets a
      // custom nudge delay, tuned for a reasonable trade-off between traffic
      // and freshness.
      return kDefaultLocalChangeNudgeDelayForSessions;
    case SAVED_TAB_GROUP:
      return syncer::kTabGroupsSaveCustomNudgeDelay.Get();
    case BOOKMARKS:
    case PREFERENCES:
    case PRODUCT_COMPARISON:
      // Types with sometimes automatic changes get longer delays to allow more
      // coalescing.
      return kBigLocalChangeNudgeDelay;
    case OUTGOING_PASSWORD_SHARING_INVITATION:
    case SHARING_MESSAGE:
      // Sharing messages are time-sensitive, so use a small nudge delay.
      return kMinLocalChangeNudgeDelay;
    case PASSWORDS:
    case AUTOFILL_PROFILE:
    case AUTOFILL_WALLET_CREDENTIAL:
    case AUTOFILL_WALLET_DATA:
    case AUTOFILL_WALLET_METADATA:
    case AUTOFILL_WALLET_OFFER:
    case AUTOFILL_WALLET_USAGE:
    case COLLABORATION_GROUP:
    case CONTACT_INFO:
    case THEMES:
    case EXTENSIONS:
    case SEARCH_ENGINES:
    case APPS:
    case APP_SETTINGS:
    case EXTENSION_SETTINGS:
    case HISTORY_DELETE_DIRECTIVES:
    case DICTIONARY:
    case DEVICE_INFO:
    case INCOMING_PASSWORD_SHARING_INVITATION:
    case PRIORITY_PREFERENCES:
    case SUPERVISED_USER_SETTINGS:
    case APP_LIST:
    case ARC_PACKAGE:
    case PRINTERS:
    case PRINTERS_AUTHORIZATION_SERVERS:
    case READING_LIST:
    case USER_CONSENTS:
    case SEND_TAB_TO_SELF:
    case SECURITY_EVENTS:
    case SHARED_TAB_GROUP_DATA:
    case WIFI_CONFIGURATIONS:
    case WEB_APPS:
    case WEB_APKS:
    case OS_PREFERENCES:
    case OS_PRIORITY_PREFERENCES:
    case WORKSPACE_DESK:
    case NIGORI:
    case POWER_BOOKMARK:
    case WEBAUTHN_CREDENTIAL:
    case PLUS_ADDRESS:
    case PLUS_ADDRESS_SETTING:
      return kMediumLocalChangeNudgeDelay;
    case UNSPECIFIED:
      NOTREACHED_IN_MIGRATION();
      return base::TimeDelta();
  }
}

bool CanGetCommitsFromExtensions(DataType data_type) {
  switch (data_type) {
    // For these types, extensions can trigger unlimited commits via a js API.
    case BOOKMARKS:                  // chrome.bookmarks API.
    case EXTENSION_SETTINGS:         // chrome.storage.sync API.
    case APP_SETTINGS:               // chrome.storage.sync API.
    case HISTORY_DELETE_DIRECTIVES:  // chrome.history and chrome.browsingData.
    // Accessible via navigator.credentials to both extensions and sites.
    case WEBAUTHN_CREDENTIAL:
      return true;
    // For these types, extensions can delete existing data using a js API.
    // However, as they cannot generate new entities, the number of deletions is
    // limited by the number of entities previously manually added by the user.
    // Thus, there's no need to apply quota to these deletions.
    case PASSWORDS:         // chrome.browsingData API.
    case AUTOFILL:          // chrome.browsingData API.
    case AUTOFILL_PROFILE:  // chrome.browsingData API.
    case CONTACT_INFO:      // chrome.browsingData API.
    // All the remaining types are not affected by any extension js API.
    case USER_EVENTS:
    case SESSIONS:
    case PREFERENCES:
    case SHARING_MESSAGE:
    case AUTOFILL_WALLET_CREDENTIAL:
    case AUTOFILL_WALLET_DATA:
    case AUTOFILL_WALLET_METADATA:
    case AUTOFILL_WALLET_OFFER:
    case AUTOFILL_WALLET_USAGE:
    case THEMES:
    case EXTENSIONS:
    case SEARCH_ENGINES:
    case APPS:
    case HISTORY:
    case DICTIONARY:
    case DEVICE_INFO:
    case PRIORITY_PREFERENCES:
    case SUPERVISED_USER_SETTINGS:
    case APP_LIST:
    case ARC_PACKAGE:
    case PRINTERS:
    case PRINTERS_AUTHORIZATION_SERVERS:
    case READING_LIST:
    case USER_CONSENTS:
    case SEND_TAB_TO_SELF:
    case SECURITY_EVENTS:
    case WIFI_CONFIGURATIONS:
    case WEB_APPS:
    case WEB_APKS:
    case OS_PREFERENCES:
    case OS_PRIORITY_PREFERENCES:
    case WORKSPACE_DESK:
    case NIGORI:
    case SAVED_TAB_GROUP:
    case POWER_BOOKMARK:
    case INCOMING_PASSWORD_SHARING_INVITATION:
    case OUTGOING_PASSWORD_SHARING_INVITATION:
    case SHARED_TAB_GROUP_DATA:
    case COLLABORATION_GROUP:
    case PLUS_ADDRESS:
    case PLUS_ADDRESS_SETTING:
    case PRODUCT_COMPARISON:
    case COOKIES:
      return false;
    case UNSPECIFIED:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

}  // namespace

WaitInterval::WaitInterval(BlockingMode mode, base::TimeDelta length)
    : mode(mode), length(length) {}

WaitInterval::~WaitInterval() = default;

DataTypeTracker::DataTypeTracker(DataType type)
    : type_(type),
      local_change_nudge_delay_(GetDefaultLocalChangeNudgeDelay(type)),
      quota_(
          CanGetCommitsFromExtensions(type)
              ? std::make_unique<CommitQuota>(kInitialTokensForExtensionTypes,
                                              kRefillIntervalForExtensionTypes)
              : nullptr),
      depleted_quota_nudge_delay_(kDepletedQuotaNudgeDelayForExtensionTypes) {
  // Sanity check the hardcode value for kMinLocalChangeNudgeDelay.
  DCHECK_GE(local_change_nudge_delay_, kMinLocalChangeNudgeDelay);
}

DataTypeTracker::~DataTypeTracker() = default;

void DataTypeTracker::RecordLocalChange() {
  local_nudge_count_++;
}

void DataTypeTracker::RecordLocalRefreshRequest() {
  local_refresh_request_count_++;
}

void DataTypeTracker::RecordInitialSyncRequired() {
  initial_sync_required_ = true;
}

void DataTypeTracker::RecordCommitConflict() {
  sync_required_to_resolve_conflict_ = true;
}

void DataTypeTracker::RecordSuccessfulCommitMessage() {
  if (quota_) {
    quota_->ConsumeToken();
    if (!quota_->HasTokensAvailable()) {
      base::UmaHistogramEnumeration(
          "Sync.DataTypeCommitMessageHasDepletedQuota",
          DataTypeHistogramValue(type_));
    }
  }
}

void DataTypeTracker::RecordSuccessfulSyncCycleIfNotBlocked() {
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

bool DataTypeTracker::IsSyncRequired() const {
  return !IsBlocked() && (HasLocalChangePending() || IsGetUpdatesRequired());
}

bool DataTypeTracker::IsGetUpdatesRequired() const {
  // TODO(crbug.com/40611499): Maybe this shouldn't check
  // IsInitialSyncRequired(): The initial sync is done in a configuration cycle,
  // while this method refers to normal cycles.
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
  return has_pending_invalidations_;
}

bool DataTypeTracker::IsInitialSyncRequired() const {
  return initial_sync_required_;
}

bool DataTypeTracker::IsSyncRequiredToResolveConflict() const {
  return sync_required_to_resolve_conflict_;
}

void DataTypeTracker::FillGetUpdatesTriggersMessage(
    sync_pb::GetUpdateTriggers* msg) {
  msg->set_local_modification_nudges(local_nudge_count_);
  msg->set_datatype_refresh_nudges(local_refresh_request_count_);
  msg->set_initial_sync_in_progress(initial_sync_required_);
  msg->set_sync_for_resolve_conflict_in_progress(
      sync_required_to_resolve_conflict_);
}

bool DataTypeTracker::IsBlocked() const {
  return wait_interval_.get() &&
         (wait_interval_->mode == WaitInterval::BlockingMode::kThrottled ||
          wait_interval_->mode ==
              WaitInterval::BlockingMode::kExponentialBackoff);
}

base::TimeDelta DataTypeTracker::GetTimeUntilUnblock() const {
  DCHECK(IsBlocked());
  return std::max(base::Seconds(0), unblock_time_ - base::TimeTicks::Now());
}

base::TimeDelta DataTypeTracker::GetLastBackoffInterval() const {
  if (GetBlockingMode() !=
      WaitInterval::BlockingMode::kExponentialBackoffRetrying) {
    NOTREACHED_IN_MIGRATION();
    return base::Seconds(0);
  }
  return wait_interval_->length;
}

void DataTypeTracker::ThrottleType(base::TimeDelta duration,
                                   base::TimeTicks now) {
  unblock_time_ = std::max(unblock_time_, now + duration);
  wait_interval_ = std::make_unique<WaitInterval>(
      WaitInterval::BlockingMode::kThrottled, duration);
}

void DataTypeTracker::BackOffType(base::TimeDelta duration,
                                  base::TimeTicks now) {
  unblock_time_ = std::max(unblock_time_, now + duration);
  wait_interval_ = std::make_unique<WaitInterval>(
      WaitInterval::BlockingMode::kExponentialBackoff, duration);
}

void DataTypeTracker::UpdateThrottleOrBackoffState() {
  if (base::TimeTicks::Now() >= unblock_time_) {
    if (wait_interval_.get() &&
        (wait_interval_->mode ==
             WaitInterval::BlockingMode::kExponentialBackoff ||
         wait_interval_->mode ==
             WaitInterval::BlockingMode::kExponentialBackoffRetrying)) {
      wait_interval_->mode =
          WaitInterval::BlockingMode::kExponentialBackoffRetrying;
    } else {
      unblock_time_ = base::TimeTicks();
      wait_interval_.reset();
    }
  }
}

void DataTypeTracker::SetHasPendingInvalidations(
    bool has_pending_invalidations) {
  has_pending_invalidations_ = has_pending_invalidations;
}

void DataTypeTracker::UpdateLocalChangeNudgeDelay(base::TimeDelta delay) {
  // Protect against delays too small being set.
  if (delay >= kMinLocalChangeNudgeDelay) {
    local_change_nudge_delay_ = delay;
  }
}

base::TimeDelta DataTypeTracker::GetLocalChangeNudgeDelay(
    bool is_single_client) const {
  if (quota_ && !quota_->HasTokensAvailable()) {
    base::UmaHistogramEnumeration("Sync.DataTypeCommitWithDepletedQuota",
                                  DataTypeHistogramValue(type_));
    return depleted_quota_nudge_delay_;
  }
  base::TimeDelta result = local_change_nudge_delay_;
  if (is_single_client &&
      base::FeatureList::IsEnabled(kSyncIncreaseNudgeDelayForSingleClient)) {
    result *= kSyncIncreaseNudgeDelayForSingleClientFactor.Get();
  }
  return result;
}

base::TimeDelta DataTypeTracker::GetRemoteInvalidationDelay() const {
  if (quota_ && !quota_->HasTokensAvailable()) {
    // Using the extended nudge delay for remote invalidations makes sure that
    // two devices on a commit spree (e.g. through the same extension) don't
    // have an escape hatch from the extended nudge delay by sending
    // invalidations to each other.
    return depleted_quota_nudge_delay_;
  }
  return kRemoteInvalidationDelay;
}

WaitInterval::BlockingMode DataTypeTracker::GetBlockingMode() const {
  if (!wait_interval_) {
    return WaitInterval::BlockingMode::kUnknown;
  }
  return wait_interval_->mode;
}

void DataTypeTracker::SetLocalChangeNudgeDelayIgnoringMinForTest(
    base::TimeDelta delay) {
  local_change_nudge_delay_ = delay;
}

void DataTypeTracker::SetQuotaParamsIfExtensionType(
    std::optional<int> max_tokens,
    std::optional<base::TimeDelta> refill_interval,
    std::optional<base::TimeDelta> depleted_quota_nudge_delay) {
  if (!quota_) {
    return;
  }
  depleted_quota_nudge_delay_ = depleted_quota_nudge_delay.value_or(
      kDepletedQuotaNudgeDelayForExtensionTypes);
  quota_->SetParams(max_tokens.value_or(kInitialTokensForExtensionTypes),
                    refill_interval.value_or(kRefillIntervalForExtensionTypes));
}

}  // namespace syncer
