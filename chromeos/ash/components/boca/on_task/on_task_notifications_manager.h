// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ON_TASK_NOTIFICATIONS_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ON_TASK_NOTIFICATIONS_MANAGER_H_

#include <map>
#include <memory>
#include <string>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/system/toast_data.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/delayed_task_handle.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/boca/on_task/on_task_notification_blocker.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace ash::boca {

// Notifications manager implementation that is primarily used for triggering
// and managing OnTask specific toasts and notifications.
class OnTaskNotificationsManager {
 public:
  // Encapsulation of params needed for toast creation. This also includes
  // the completion callback that is triggered after the countdown ends.
  // TODO (crbug.com/376170581): Countdown helpers should be associated with
  // notifications instead.
  struct ToastCreateParams {
    ToastCreateParams(
        std::string id,
        ToastCatalogName catalog_name,
        base::RepeatingCallback<std::u16string(base::TimeDelta)>
            text_description_callback,
        base::RepeatingClosure completion_callback = base::DoNothing(),
        base::TimeDelta countdown_period =
            base::Seconds(1));  // Show toast for 1 second at the very least.
    ToastCreateParams(const ToastCreateParams& other);
    ToastCreateParams& operator=(const ToastCreateParams& other);
    ToastCreateParams(ToastCreateParams&& other);
    ToastCreateParams& operator=(ToastCreateParams&& other);
    ~ToastCreateParams();

    std::string id;
    ToastCatalogName catalog_name;
    base::RepeatingCallback<std::u16string(base::TimeDelta)>
        text_description_callback;
    base::RepeatingClosure completion_callback;
    base::TimeDelta countdown_period;
  };

  // Encapsulation of params needed for creating notifications.
  struct NotificationCreateParams {
    NotificationCreateParams(std::string id,
                             std::u16string title,
                             std::u16string message,
                             message_center::NotifierId notifier_id);
    NotificationCreateParams(const NotificationCreateParams& other);
    NotificationCreateParams& operator=(const NotificationCreateParams& other);
    NotificationCreateParams(NotificationCreateParams&& other);
    NotificationCreateParams& operator=(NotificationCreateParams&& other);
    ~NotificationCreateParams();

    std::string id;
    std::u16string title;
    std::u16string message;
    message_center::NotifierId notifier_id;
  };

  // Delegate implementation that can be overridden by tests to stub toast
  // display actions. Especially relevant for tests that cannot leverage Ash UI.
  class Delegate {
   public:
    Delegate() = default;
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;
    virtual ~Delegate() = default;

    // Display toast using the specified data.
    virtual void ShowToast(ToastData toast_data);

    // Display specified notification.
    virtual void ShowNotification(
        std::unique_ptr<message_center::Notification> notification);

    // Clears the notification with the specified id, if it exists.
    virtual void ClearNotification(const std::string& notification_id);
  };

  // Static factory helpers.
  static std::unique_ptr<OnTaskNotificationsManager> Create();
  static std::unique_ptr<OnTaskNotificationsManager> CreateForTest(
      std::unique_ptr<Delegate> delegate);

  OnTaskNotificationsManager(const OnTaskNotificationsManager&) = delete;
  OnTaskNotificationsManager& operator=(const OnTaskNotificationsManager&) =
      delete;
  ~OnTaskNotificationsManager();

  // Creates a toast using the specified params. Includes a one second timer
  // delay before the toast is surfaced.
  void CreateToast(ToastCreateParams params);

  // Creates a notification using the specified params.
  void CreateNotification(NotificationCreateParams params);

  // Stops processing of the specified notification. Normally triggered when the
  // notification has been processed or when there is an override.
  void StopProcessingNotification(const std::string& notification_id);

  // Prepare notification manager for the specified locked mode. Includes
  // setting up the notification blocker to prevent surfacing notifications that
  // possibly allow users to exit this mode.
  void ConfigureForLockedMode(bool locked);

  // Retrieves the notification blocker for testing purposes.
  OnTaskNotificationBlocker* GetNotificationBlockerForTesting();

 private:
  explicit OnTaskNotificationsManager(std::unique_ptr<Delegate> delegate);

  // Internal helper used by the timer to surface toast notifications
  // periodically.
  void CreateToastInternal(ToastCreateParams& params);

  SEQUENCE_CHECKER(sequence_checker_);
  const std::unique_ptr<Delegate> delegate_;
  std::map<std::string, std::unique_ptr<base::RepeatingTimer>>
      pending_notifications_map_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<OnTaskNotificationBlocker> notification_blocker_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<OnTaskNotificationsManager> weak_ptr_factory_{this};
};

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ON_TASK_NOTIFICATIONS_MANAGER_H_
