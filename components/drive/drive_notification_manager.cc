// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/drive/drive_notification_manager.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "components/drive/drive_notification_observer.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/invalidation/public/topic_invalidation_map.h"

namespace drive {

namespace {

// The polling interval time is used when XMPP is disabled.
const int kFastPollingIntervalInSecs = 60;

// The polling interval time is used when XMPP is enabled.  Theoretically
// polling should be unnecessary if XMPP is enabled, but just in case.
const int kSlowPollingIntervalInSecs = 3600;

// The period to batch together invalidations before passing them to observers.
constexpr int kInvalidationBatchIntervalSecs = 15;

// The sync invalidation Topic name for Google Drive.
const char kDriveInvalidationTopicName[] = "Drive";

// Team drive invalidation ID's from FCM are "team-drive-<team_drive_id>".
constexpr char kTeamDriveChangePrefix[] = "team-drive-";

}  // namespace

DriveNotificationManager::DriveNotificationManager(
    invalidation::InvalidationService* invalidation_service,
    const base::TickClock* clock)
    : invalidation_service_(invalidation_service),
      push_notification_registered_(false),
      push_notification_enabled_(false),
      observers_notified_(false),
      batch_timer_(clock) {
  DCHECK(invalidation_service_);
  RegisterDriveNotifications();
}

DriveNotificationManager::~DriveNotificationManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void DriveNotificationManager::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Unregister for Drive notifications.
  if (!invalidation_service_ || !push_notification_registered_)
    return;

  // We unregister the handler without updating unregistering our IDs on
  // purpose.  See the class comment on the InvalidationService interface for
  // more information.
  invalidation_service_->UnregisterInvalidationHandler(this);
  invalidation_service_ = nullptr;
}

void DriveNotificationManager::OnInvalidatorStateChange(
    invalidation::InvalidatorState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  push_notification_enabled_ = (state == invalidation::INVALIDATIONS_ENABLED);
  if (push_notification_enabled_) {
    DVLOG(1) << "XMPP Notifications enabled";
  } else {
    DVLOG(1) << "XMPP Notifications disabled (state=" << state << ")";
  }
  for (auto& observer : observers_)
    observer.OnPushNotificationEnabled(push_notification_enabled_);
}

void DriveNotificationManager::OnIncomingInvalidation(
    const invalidation::TopicInvalidationMap& invalidation_map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << "XMPP Drive Notification Received";

  for (const auto& topic : invalidation_map.GetTopics()) {
    // Empty string indicates default change list.
    std::string unpacked_id;
    if (topic != GetDriveInvalidationTopic()) {
      unpacked_id = ExtractTeamDriveId(topic);
      DCHECK(!unpacked_id.empty()) << "Unexpected topic " << topic;
    }
    auto invalidations = invalidation_map.ForTopic(topic);
    int64_t& invalidation_version =
        invalidated_change_ids_.emplace(unpacked_id, -1).first->second;
    for (auto& invalidation : invalidations) {
      if (!invalidation.is_unknown_version() &&
          invalidation.version() > invalidation_version) {
        invalidation_version = invalidation.version();
      }
    }
  }

  // This effectively disables 'local acks'.  It tells the invalidations system
  // to not bother saving invalidations across restarts for us.
  // See crbug.com/320878.
  invalidation_map.AcknowledgeAll();

  if (!batch_timer_.IsRunning() && !invalidated_change_ids_.empty()) {
    // Stop the polling timer as we'll be sending a batch soon.
    polling_timer_.Stop();

    // Restart the timer to send the batch when the timer fires.
    RestartBatchTimer();
  }
}

std::string DriveNotificationManager::GetOwnerName() const { return "Drive"; }
bool DriveNotificationManager::IsPublicTopic(
    const invalidation::Topic& topic) const {
  return base::StartsWith(topic, kTeamDriveChangePrefix);
}

void DriveNotificationManager::AddObserver(
    DriveNotificationObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (observers_.empty()) {
    UpdateRegisteredDriveNotifications();
    RestartPollingTimer();
  }

  observers_.AddObserver(observer);
}

void DriveNotificationManager::RemoveObserver(
    DriveNotificationObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);

  if (observers_.empty()) {
    CHECK(invalidation_service_->UpdateInterestedTopics(
        this, invalidation::TopicSet()));
    polling_timer_.Stop();
    batch_timer_.Stop();
    invalidated_change_ids_.clear();
  }
}

void DriveNotificationManager::UpdateTeamDriveIds(
    const std::set<std::string>& added_team_drive_ids,
    const std::set<std::string>& removed_team_drive_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // We only want to update the invalidation service if we actually change the
  // set of team drive id's we're currently registered for.
  bool set_changed = false;

  for (const auto& added : added_team_drive_ids) {
    if (team_drive_ids_.insert(added).second) {
      set_changed = true;
    }
  }

  for (const auto& removed : removed_team_drive_ids) {
    if (team_drive_ids_.erase(removed)) {
      set_changed = true;
    }
  }

  if (set_changed && !observers_.empty()) {
    UpdateRegisteredDriveNotifications();
  }
}

void DriveNotificationManager::ClearTeamDriveIds() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!team_drive_ids_.empty()) {
    team_drive_ids_.clear();
    if (!observers_.empty()) {
      UpdateRegisteredDriveNotifications();
    }
  }
}

void DriveNotificationManager::RestartPollingTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const int interval_secs = (push_notification_enabled_ ?
                             kSlowPollingIntervalInSecs :
                             kFastPollingIntervalInSecs);

  int jitter = base::RandInt(0, interval_secs);

  polling_timer_.Stop();
  polling_timer_.Start(
      FROM_HERE, base::Seconds(interval_secs + jitter),
      base::BindOnce(&DriveNotificationManager::NotifyObserversToUpdate,
                     weak_ptr_factory_.GetWeakPtr(), NOTIFICATION_POLLING,
                     std::map<std::string, int64_t>()));
}

void DriveNotificationManager::RestartBatchTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int jitter = base::RandInt(0, kInvalidationBatchIntervalSecs);

  batch_timer_.Stop();
  batch_timer_.Start(
      FROM_HERE, base::Seconds(kInvalidationBatchIntervalSecs + jitter),
      base::BindOnce(&DriveNotificationManager::OnBatchTimerExpired,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DriveNotificationManager::NotifyObserversToUpdate(
    NotificationSource source,
    std::map<std::string, int64_t> invalidations) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "Notifying observers: " << NotificationSourceToString(source);

  if (source == NOTIFICATION_XMPP) {
    auto my_drive_invalidation = invalidations.find("");
    if (my_drive_invalidation != invalidations.end() &&
        my_drive_invalidation->second != -1) {
      // The invalidation version for My Drive is smaller than what's expected
      // for fetch requests by 1. Increment it unless it hasn't been set.
      ++my_drive_invalidation->second;
    }
    for (auto& observer : observers_)
      observer.OnNotificationReceived(invalidations);
  } else {
    for (auto& observer : observers_)
      observer.OnNotificationTimerFired();
  }
  if (!observers_notified_) {
    UMA_HISTOGRAM_BOOLEAN("Drive.PushNotificationInitiallyEnabled",
                          push_notification_enabled_);
  }
  observers_notified_ = true;

  // Note that polling_timer_ is not a repeating timer. Restarting manually
  // here is better as XMPP may be received right before the polling timer is
  // fired (i.e. we don't notify observers twice in a row).
  RestartPollingTimer();
}

void DriveNotificationManager::RegisterDriveNotifications() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!push_notification_enabled_);

  if (!invalidation_service_)
    return;

  invalidation_service_->RegisterInvalidationHandler(this);

  push_notification_registered_ = true;

  UMA_HISTOGRAM_BOOLEAN("Drive.PushNotificationRegistered",
                        push_notification_registered_);
}

void DriveNotificationManager::UpdateRegisteredDriveNotifications() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!invalidation_service_)
    return;

  invalidation::TopicSet topics;
  topics.insert(GetDriveInvalidationTopic());

  for (const auto& team_drive_id : team_drive_ids_) {
    topics.insert(GetTeamDriveInvalidationTopic(team_drive_id));
  }

  CHECK(invalidation_service_->UpdateInterestedTopics(this, topics));
  OnInvalidatorStateChange(invalidation_service_->GetInvalidatorState());
}

void DriveNotificationManager::OnBatchTimerExpired() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::map<std::string, int64_t> change_ids_to_update;
  invalidated_change_ids_.swap(change_ids_to_update);
  if (!change_ids_to_update.empty()) {
    NotifyObserversToUpdate(NOTIFICATION_XMPP, std::move(change_ids_to_update));
  }
}

// static
std::string DriveNotificationManager::NotificationSourceToString(
    NotificationSource source) {
  switch (source) {
    case NOTIFICATION_XMPP:
      return "NOTIFICATION_XMPP";
    case NOTIFICATION_POLLING:
      return "NOTIFICATION_POLLING";
  }

  NOTREACHED();
  return "";
}

invalidation::Topic DriveNotificationManager::GetDriveInvalidationTopic()
    const {
  return kDriveInvalidationTopicName;
}

invalidation::Topic DriveNotificationManager::GetTeamDriveInvalidationTopic(
    const std::string& team_drive_id) const {
  return base::StrCat({kTeamDriveChangePrefix, team_drive_id});
}

std::string DriveNotificationManager::ExtractTeamDriveId(
    base::StringPiece topic_name) const {
  base::StringPiece prefix = kTeamDriveChangePrefix;
  if (!base::StartsWith(topic_name, prefix)) {
    return {};
  }
  return std::string(topic_name.substr(prefix.size()));
}

}  // namespace drive
