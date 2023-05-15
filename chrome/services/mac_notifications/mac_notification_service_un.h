// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MAC_NOTIFICATIONS_MAC_NOTIFICATION_SERVICE_UN_H_
#define CHROME_SERVICES_MAC_NOTIFICATIONS_MAC_NOTIFICATION_SERVICE_UN_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/common/notifications/notification_image_retainer.h"
#import "chrome/services/mac_notifications/notification_category_manager.h"
#include "chrome/services/mac_notifications/public/mojom/mac_notifications.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@class AlertUNNotificationCenterDelegate;
@class UNUserNotificationCenter;

namespace mac_notifications {

// Implementation of the MacNotificationService mojo interface using the
// UNNotification system API.
class API_AVAILABLE(macos(10.14)) MacNotificationServiceUN
    : public mojom::MacNotificationService {
 public:
  // Timer interval used to synchronize displayed notifications.
  static constexpr auto kSynchronizationInterval = base::Minutes(10);

  MacNotificationServiceUN(
      mojo::PendingReceiver<mojom::MacNotificationService> service,
      mojo::PendingRemote<mojom::MacNotificationActionHandler> handler,
      UNUserNotificationCenter* notification_center);
  MacNotificationServiceUN(const MacNotificationServiceUN&) = delete;
  MacNotificationServiceUN& operator=(const MacNotificationServiceUN&) = delete;
  ~MacNotificationServiceUN() override;

  // mojom::MacNotificationService:
  void DisplayNotification(mojom::NotificationPtr notification) override;
  void GetDisplayedNotifications(
      mojom::ProfileIdentifierPtr profile,
      GetDisplayedNotificationsCallback callback) override;
  void CloseNotification(mojom::NotificationIdentifierPtr identifier) override;
  void CloseNotificationsForProfile(
      mojom::ProfileIdentifierPtr profile) override;
  void CloseAllNotifications() override;

 private:
  // Requests notification permissions from the system. This will ask the user
  // to accept permissions if not granted or denied already.
  void RequestPermission();

  // Initializes the |delivered_notifications_| with notifications currently
  // shown in the macOS notification center.
  void InitializeDeliveredNotifications(base::OnceClosure callback);
  void DoInitializeDeliveredNotifications(
      base::OnceClosure callback,
      NSArray<UNNotification*>* notifications,
      NSSet<UNNotificationCategory*>* categories);

  // Called regularly while we think that notifications are on screen to detect
  // when they get closed.
  void ScheduleSynchronizeNotifications();
  void DoSynchronizeNotifications(
      std::vector<mojom::NotificationIdentifierPtr> notifications);

  // Called by |delegate_| when a user interacts with a notification.
  void OnNotificationAction(mojom::NotificationActionInfoPtr action);

  // Called when the notifications got closed for any reason.
  void OnNotificationsClosed(const std::vector<std::string>& notification_ids);

  mojo::Receiver<mojom::MacNotificationService> binding_;
  mojo::Remote<mojom::MacNotificationActionHandler> action_handler_;
  AlertUNNotificationCenterDelegate* __strong delegate_;
  UNUserNotificationCenter* __strong notification_center_;

  // Category manager for action buttons.
  NotificationCategoryManager category_manager_;
  // Image retainer to pass image attachments to notifications.
  NotificationImageRetainer image_retainer_;

  // Keeps track of delivered notifications to detect closed notifications.
  base::flat_map<std::string, mojom::NotificationMetadataPtr>
      delivered_notifications_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::RepeatingTimer synchronize_displayed_notifications_timer_;

  // Ensures that the methods in this class are called on the same sequence.
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<MacNotificationServiceUN> weak_factory_{this};
};

}  // namespace mac_notifications

#endif  // CHROME_SERVICES_MAC_NOTIFICATIONS_MAC_NOTIFICATION_SERVICE_UN_H_
