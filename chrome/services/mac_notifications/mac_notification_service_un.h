// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MAC_NOTIFICATIONS_MAC_NOTIFICATION_SERVICE_UN_H_
#define CHROME_SERVICES_MAC_NOTIFICATIONS_MAC_NOTIFICATION_SERVICE_UN_H_

#import <UserNotifications/UserNotifications.h>

#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#import "chrome/services/mac_notifications/notification_category_manager.h"
#include "chrome/services/mac_notifications/public/mojom/mac_notifications.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

@class AlertUNNotificationCenterDelegate;

namespace mac_notifications {

// Implementation of the MacNotificationService mojo interface using the
// UNNotification system API.
class MacNotificationServiceUN : public mojom::MacNotificationService {
 public:
  // Timer interval used to synchronize displayed notifications.
  static constexpr auto kSynchronizationInterval = base::Minutes(10);

  MacNotificationServiceUN(
      mojo::PendingRemote<mojom::MacNotificationActionHandler> handler,
      base::RepeatingCallback<void(mojom::PermissionStatus)>
          permission_status_changed_callback,
      UNUserNotificationCenter* notification_center);
  MacNotificationServiceUN(const MacNotificationServiceUN&) = delete;
  MacNotificationServiceUN& operator=(const MacNotificationServiceUN&) = delete;
  ~MacNotificationServiceUN() override;

  // Binds or re-binds the notification service mojo receiver. If already bound,
  // this replaces the existing binding with the newly passed in one.
  void Bind(mojo::PendingReceiver<mojom::MacNotificationService> service);

  // Requests notification permissions from the system. This will ask the user
  // to accept permissions if not granted or denied already, and open macOS
  // System Settings to the app's notification settings pane if permissions are
  // not already granted. If a permission request is already pending, the
  // `callback` is run when that request completes.
  using RequestPermissionCallback =
      base::OnceCallback<void(mojom::RequestPermissionResult)>;
  void RequestPermission(RequestPermissionCallback callback);

  // Overrides the default behavior of opening macOS System Settings during
  // `RequestPermission()` in tests.
  static void SetOpenSystemSettingsCallbackForTesting(
      base::RepeatingClosure callback);

  // mojom::MacNotificationService:
  void DisplayNotification(mojom::NotificationPtr notification) override;
  void GetDisplayedNotifications(
      mojom::ProfileIdentifierPtr profile,
      const std::optional<GURL>& origin,
      GetDisplayedNotificationsCallback callback) override;
  void CloseNotification(mojom::NotificationIdentifierPtr identifier) override;
  void CloseNotificationsForProfile(
      mojom::ProfileIdentifierPtr profile) override;
  void CloseAllNotifications() override;
  void OkayToTerminateService(OkayToTerminateServiceCallback callback) override;

  // Returns true if we recently (in the last 100ms) handled a "default" click
  // action for a notification. This can be used to ignore the "re-open" event
  // that gets send to an application shortly afterwards.
  bool DidRecentlyHandleClickAction() const;

 private:
  // Holds state for an in-flight notification delivery request (including its
  // temporary icon file and attachment-retry state) across asynchronous
  // `UNUserNotificationCenter` completion callbacks.
  struct PendingDelivery;

  struct DeliveredNotification {
    mojom::NotificationMetadataPtr meta;
    // Non-nil while `addNotificationRequest:` or
    // `replaceContentForRequestWithIdentifier:` (including any attachment-free
    // retry) is in flight for this notification. Used both to prevent a
    // concurrent `SynchronizeNotifications()` from treating an in-flight
    // notification as closed, and to ignore stale completion callbacks if the
    // same notification ID is re-displayed while an earlier delivery is still
    // in flight.
    UNMutableNotificationContent* __strong pending_content;
    // Set when a `renotify = false` replacement is waiting for
    // `SynchronizeNotifications` to finish before `DoDisplayNotification` runs,
    // so `DoSynchronizeNotifications` removes a missing previous entry without
    // emitting a `kClose` action.
    bool is_being_replaced = false;
  };

  void DoDisplayNotification(mojom::NotificationPtr notification);
  void DeliverNotificationContent(PendingDelivery delivery);
  void OnNotificationDelivered(PendingDelivery delivery, NSError* error);

  void ReportRequestPermissionResult(mojom::RequestPermissionResult result);
  void DoRequestPermission();
  void OpenSystemSettings();

  // Initializes the |delivered_notifications_| with notifications currently
  // shown in the macOS notification center.
  void InitializeDeliveredNotifications();
  void DoInitializeDeliveredNotifications(
      NSArray<UNNotification*>* notifications,
      NSSet<UNNotificationCategory*>* categories);

  // Called regularly while we think that notifications are on screen to detect
  // when they get closed.
  void ScheduleSynchronizeNotifications();
  void SynchronizeNotifications(base::OnceClosure done);
  void DoSynchronizeNotifications(
      std::vector<mojom::NotificationIdentifierPtr> notifications);

  void SynchronizePermissionStatus(bool log_result);

  // Called when the application becomes active. macOS doesn't notify apps when
  // their notification permission changes, so this is a good moment to poll
  // for changes the user might have made in System Settings.
  void OnApplicationDidBecomeActive();

  // Called by |delegate_| when a user interacts with a notification.
  void OnNotificationAction(mojom::NotificationActionInfoPtr action);

  // Called when the notifications got closed for any reason.
  void OnNotificationsClosed(const std::vector<std::string>& notification_ids);

  // Called when we got an updated UNAuthorizationStatus.
  void OnGotAuthorizationStatus(UNAuthorizationStatus status);

  mojo::Receiver<mojom::MacNotificationService> binding_;
  mojo::Remote<mojom::MacNotificationActionHandler> action_handler_;
  AlertUNNotificationCenterDelegate* __strong delegate_;
  UNUserNotificationCenter* __strong notification_center_;
  // Observer token for NSApplicationDidBecomeActiveNotification.
  id<NSObject> __strong did_become_active_observer_;

  // Set to true when initialization has finished, and this service is ready
  // to receive mojo calls. `binding_` will not be bound until this happens.
  bool finished_initialization_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  // If set, this callback is called when initialization completes.
  base::OnceClosure after_initialization_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Category manager for action buttons.
  NotificationCategoryManager category_manager_;

  // Keeps track of delivered notifications to detect closed notifications.
  base::flat_map<std::string, DeliveredNotification> delivered_notifications_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::RepeatingTimer synchronize_displayed_notifications_timer_;
  bool is_synchronizing_notifications_ = false;
  std::vector<base::OnceClosure> synchronize_notifications_done_callbacks_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Holds the callbacks for all pending RequestPermission() calls. This is
  // also used to determine if it is safe for chrome to terminate this service,
  // as we don't want to do this while there are any pending permission
  // requests.
  std::vector<RequestPermissionCallback> pending_permission_requests_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // True while an OS notification permission prompt is being requested via
  // `DoRequestPermission()`.
  bool is_permission_prompt_pending_ GUARDED_BY_CONTEXT(sequence_checker_) =
      false;

  // Callback to be called any time the system level permission status changes.
  base::RepeatingCallback<void(mojom::PermissionStatus)>
      permission_status_changed_callback_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Last value that was passed to `permission_status_changed_callback_` to
  // make sure we only call the callback when the value changes.
  std::optional<mojom::PermissionStatus> last_permission_status_
      GUARDED_BY_CONTEXT(sequence_checker_);
  bool is_synchronizing_permission_status_
      GUARDED_BY_CONTEXT(sequence_checker_) = false;

  // Ensures that the methods in this class are called on the same sequence.
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<MacNotificationServiceUN> weak_factory_{this};
};

}  // namespace mac_notifications

#endif  // CHROME_SERVICES_MAC_NOTIFICATIONS_MAC_NOTIFICATION_SERVICE_UN_H_
