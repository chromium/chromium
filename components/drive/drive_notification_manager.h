// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DRIVE_DRIVE_NOTIFICATION_MANAGER_H_
#define COMPONENTS_DRIVE_DRIVE_NOTIFICATION_MANAGER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/time/default_tick_clock.h"
#include "base/timer/timer.h"
#include "components/drive/drive_notification_observer.h"
#include "components/invalidation/public/invalidation_handler.h"
#include "components/keyed_service/core/keyed_service.h"

namespace invalidation {
class InvalidationService;
}  // namespace invalidation

namespace drive {

// Informs observers when they should check Google Drive for updates.
// Conditions under which updates should be searched:
// 1. XMPP invalidation is received from Google Drive.
// 2. Polling timer counts down.
class DriveNotificationManager : public KeyedService,
                                 public syncer::InvalidationHandler {
 public:
  // |clock| can be injected for testing.
  explicit DriveNotificationManager(
      invalidation::InvalidationService* invalidation_service,
      bool use_fcm_object_ids = false,
      const base::TickClock* clock = base::DefaultTickClock::GetInstance());
  ~DriveNotificationManager() override;

  // KeyedService override.
  void Shutdown() override;

  // syncer::InvalidationHandler implementation.
  void OnInvalidatorStateChange(syncer::InvalidatorState state) override;
  void OnIncomingInvalidation(
      const syncer::ObjectIdInvalidationMap& invalidation_map) override;
  std::string GetOwnerName() const override;
  bool IsPublicTopic(const syncer::Topic& topic) const override;

  void AddObserver(DriveNotificationObserver* observer);
  void RemoveObserver(DriveNotificationObserver* observer);

  // There has been a change in the users team drives, and as a result we need
  // to update which objects we receive invalidations for.
  void UpdateTeamDriveIds(const std::set<std::string>& added_team_drive_ids,
                          const std::set<std::string>& removed_team_drive_ids);

  // True when XMPP notification is currently enabled.
  bool push_notification_enabled() const {
    return push_notification_enabled_;
  }

  // True when XMPP notification has been registered.
  bool push_notification_registered() const {
    return push_notification_registered_;
  }

  const std::set<std::string>& team_drive_ids_for_test() const {
    return team_drive_ids_;
  }

  const base::ObserverList<DriveNotificationObserver>::Unchecked&
  observers_for_test() {
    return observers_;
  }

 private:
  enum NotificationSource {
    NOTIFICATION_XMPP,
    NOTIFICATION_POLLING,
  };

  // Restarts the polling timer. Used for polling-based notification.
  void RestartPollingTimer();

  // Restarts the batch notification timer. Used for batching together XMPP
  // notifications so we can smooth out the traffic on the drive backends.
  void RestartBatchTimer();

  // Notifies the observers that it's time to check for updates.
  // |source| indicates where the notification comes from.
  void NotifyObserversToUpdate(NotificationSource source,
                               std::map<std::string, int64_t> invalidations);

  // Registers for Google Drive invalidation notifications through XMPP.
  void RegisterDriveNotifications();

  // Updates the list of notifications that we're expecting
  void UpdateRegisteredDriveNotifications();

  // Dispatches batched invalidations to observers.
  void OnBatchTimerExpired();

  // Returns a string representation of NotificationSource.
  static std::string NotificationSourceToString(NotificationSource source);

  std::string GetDriveInvalidationObjectId() const;
  std::string GetTeamDriveInvalidationObjectId(
      const std::string& team_drive_id) const;
  std::string ExtractTeamDriveId(base::StringPiece object_id) const;

  invalidation::InvalidationService* invalidation_service_;
  base::ObserverList<DriveNotificationObserver>::Unchecked observers_;

  // True when Drive File Sync Service is registered for Drive notifications.
  bool push_notification_registered_;
  // True if the XMPP-based push notification is currently enabled.
  bool push_notification_enabled_;
  // True once observers are notified for the first time.
  bool observers_notified_;

  // This is the set of team drive id's we're receiving notifications for.
  std::set<std::string> team_drive_ids_;

  // The timer is used for polling based notification. XMPP should usually be
  // used but notification is done per polling when XMPP is not working.
  base::OneShotTimer polling_timer_;

  // This timer is used to batch together invalidations. The invalidation
  // service can send many invalidations for the same id in rapid succession,
  // batching them together and removing duplicates is an optimzation.
  base::OneShotTimer batch_timer_;

  // The batch of invalidation id's that we've seen from the invaliation
  // service, will be reset when when send the invalidations to the observers.
  std::map<std::string, int64_t> invalidated_change_ids_;

  // Whether the FCM invalidation IDs should be used. This decides whether
  // "Drive" or "CHANGELOG" is used for the Drive invalidations and whether
  // the "TD:" or the "team-drive-" prefix is used. This value must match
  // whether |invalidation_service_| is an FCMInvalidationService.
  const bool use_fcm_object_ids_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<DriveNotificationManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DriveNotificationManager);
};

}  // namespace drive

#endif  // COMPONENTS_DRIVE_DRIVE_NOTIFICATION_MANAGER_H_
