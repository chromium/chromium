// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/on_task/on_task_notifications_manager.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/boca/on_task/notification_constants.h"
#include "chromeos/ash/components/boca/on_task/on_task_notification_blocker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "url/gurl.h"

using message_center::MessageCenter;
using message_center::Notification;
using message_center::NotificationType;
using message_center::NotifierId;
using message_center::NotifierType;
using message_center::RichNotificationData;
using message_center::SystemNotificationWarningLevel;

namespace ash::boca {

OnTaskNotificationsManager::ToastCreateParams::ToastCreateParams(
    std::string id,
    ToastCatalogName catalog_name,
    base::RepeatingCallback<std::u16string(base::TimeDelta)>
        text_description_callback,
    base::RepeatingClosure completion_callback,
    base::TimeDelta countdown_period)
    : id(id),
      catalog_name(catalog_name),
      text_description_callback(std::move(text_description_callback)),
      completion_callback(std::move(completion_callback)),
      countdown_period(countdown_period) {}

OnTaskNotificationsManager::ToastCreateParams::ToastCreateParams(
    const ToastCreateParams& other) = default;
OnTaskNotificationsManager::ToastCreateParams&
OnTaskNotificationsManager::ToastCreateParams::operator=(
    const ToastCreateParams& other) = default;
OnTaskNotificationsManager::ToastCreateParams::ToastCreateParams(
    ToastCreateParams&& other) = default;
OnTaskNotificationsManager::ToastCreateParams&
OnTaskNotificationsManager::ToastCreateParams::operator=(
    ToastCreateParams&& other) = default;

OnTaskNotificationsManager::ToastCreateParams::~ToastCreateParams() = default;

OnTaskNotificationsManager::NotificationCreateParams::NotificationCreateParams(
    std::string id,
    std::u16string title,
    int message_id,
    message_center::NotifierId notifier_id,
    base::RepeatingClosure completion_callback,
    base::TimeDelta countdown_period,
    bool is_counting_down)
    : id(id),
      title(title),
      message_id(message_id),
      notifier_id(notifier_id),
      completion_callback(std::move(completion_callback)),
      countdown_period(countdown_period),
      is_counting_down(is_counting_down) {}

OnTaskNotificationsManager::NotificationCreateParams::NotificationCreateParams(
    const NotificationCreateParams& other) = default;
OnTaskNotificationsManager::NotificationCreateParams&
OnTaskNotificationsManager::NotificationCreateParams::operator=(
    const NotificationCreateParams& other) = default;
OnTaskNotificationsManager::NotificationCreateParams::NotificationCreateParams(
    NotificationCreateParams&& other) = default;
OnTaskNotificationsManager::NotificationCreateParams&
OnTaskNotificationsManager::NotificationCreateParams::operator=(
    NotificationCreateParams&& other) = default;

OnTaskNotificationsManager::NotificationCreateParams::
    ~NotificationCreateParams() = default;

void OnTaskNotificationsManager::Delegate::ShowToast(ToastData toast_data) {
  ash::ToastManager* const toast_manager = ash::ToastManager::Get();
  CHECK(toast_manager);
  toast_manager->Show(std::move(toast_data));
}

void OnTaskNotificationsManager::Delegate::ShowNotification(
    std::unique_ptr<Notification> notification) {
  MessageCenter::Get()->AddNotification(std::move(notification));
}

void OnTaskNotificationsManager::Delegate::ClearNotification(
    const std::string& notification_id) {
  MessageCenter::Get()->RemoveNotification(notification_id, /*by_user=*/false);
}

// static
std::unique_ptr<OnTaskNotificationsManager>
OnTaskNotificationsManager::Create() {
  auto delegate = std::make_unique<OnTaskNotificationsManager::Delegate>();
  return base::WrapUnique(new OnTaskNotificationsManager(std::move(delegate)));
}

// static
std::unique_ptr<OnTaskNotificationsManager>
OnTaskNotificationsManager::CreateForTest(std::unique_ptr<Delegate> delegate) {
  return base::WrapUnique(new OnTaskNotificationsManager(std::move(delegate)));
}

OnTaskNotificationsManager::OnTaskNotificationsManager(
    std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)) {}

OnTaskNotificationsManager::~OnTaskNotificationsManager() = default;

void OnTaskNotificationsManager::CreateToast(ToastCreateParams params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pending_notifications_map_.contains(params.id)) {
    StopProcessingNotification(params.id);
  }
  auto notification_timer = std::make_unique<base::RepeatingTimer>();
  notification_timer->Start(
      FROM_HERE, kOnTaskNotificationCountdownInterval,
      base::BindRepeating(&OnTaskNotificationsManager::CreateToastInternal,
                          weak_ptr_factory_.GetWeakPtr(),
                          base::OwnedRef(params)));
  pending_notifications_map_[params.id] = std::move(notification_timer);
}

void OnTaskNotificationsManager::CreateNotification(
    NotificationCreateParams params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Clear pre-existing notifications with the same id if it exists. This is
  // to ensure they pop up when the window happens to be locked.
  ClearNotification(params.id);
  if (pending_notifications_map_.contains(params.id)) {
    StopProcessingNotification(params.id);
  }
  auto notification_timer = std::make_unique<base::RepeatingTimer>();

  // Determine the effective countdown period, using 1 second if the provided
  // period is zero since we still need to show the notification before callback
  // is triggered, otherwise using the provided period.
  const base::TimeDelta effective_countdown_period =
      params.countdown_period.is_zero() ? base::Seconds(1)
                                        : params.countdown_period;

  // Add an extra kOnTaskNotificationCountdownInterval to end_time for the timer
  // to start calling the callback.
  const base::TimeTicks end_time = effective_countdown_period +
                                   kOnTaskNotificationCountdownInterval +
                                   base::TimeTicks::Now();

  notification_timer->Start(
      FROM_HERE, kOnTaskNotificationCountdownInterval,
      base::BindRepeating(
          &OnTaskNotificationsManager::CreateNotificationInternal,
          weak_ptr_factory_.GetWeakPtr(), base::OwnedRef(params), end_time));
  pending_notifications_map_[params.id] = std::move(notification_timer);
}

void OnTaskNotificationsManager::StopProcessingNotification(
    const std::string& notification_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pending_notifications_map_.contains(notification_id)) {
    return;
  }
  pending_notifications_map_.erase(notification_id);
}

void OnTaskNotificationsManager::ClearNotification(
    const std::string& notification_id) {
  delegate_->ClearNotification(notification_id);
}

void OnTaskNotificationsManager::ConfigureForLockedMode(bool locked) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (locked && (notification_blocker_ != nullptr)) {
    // Notification blocker already set up. Return.
    return;
  }
  if (locked) {
    notification_blocker_ =
        std::make_unique<OnTaskNotificationBlocker>(MessageCenter::Get());
    notification_blocker_->Init();
  } else {
    // Clear notification blocker if the window happens to be unlocked.
    notification_blocker_.reset();
  }
}

OnTaskNotificationBlocker*
OnTaskNotificationsManager::GetNotificationBlockerForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return notification_blocker_.get();
}

void OnTaskNotificationsManager::CreateNotificationInternal(
    NotificationCreateParams& params,
    const base::TimeTicks end_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pending_notifications_map_.contains(params.id)) {
    return;
  }

  // Check if we've reached the end time and trigger callback if needed. We
  // assume the code path execution above will be less than 1s, otherwise
  // notification will not be shown.
  base::TimeDelta time_left = end_time - base::TimeTicks::Now();
  if (time_left.is_negative()) {
    params.completion_callback.Run();
    if (params.is_counting_down) {
      // When the countdown finishes, immediately clear the last countdown
      // notification.
      ClearNotification(params.id);
    }
    StopProcessingNotification(params.id);
    return;
  }

  // Display notification.
  const std::u16string message =
      params.is_counting_down
          ? l10n_util::GetStringFUTF16(
                params.message_id,
                base::NumberToString16(time_left.InSeconds() + 1))
          : l10n_util::GetStringUTF16(params.message_id);
  const gfx::VectorIcon& icon = params.is_counting_down
                                    ? ash::kNotificationTimerIcon
                                    : ash::kSecurityIcon;
  std::unique_ptr<Notification> notification = CreateSystemNotificationPtr(
      NotificationType::NOTIFICATION_TYPE_SIMPLE, params.id, params.title,
      message,
      /*display_source=*/std::u16string(),
      /*origin_url=*/GURL(), params.notifier_id,
      message_center::RichNotificationData(),
      /*delegate=*/nullptr, icon, SystemNotificationWarningLevel::NORMAL);

  // Ensure notification is visible over fullscreen windows (for example, locked
  // quiz).
  notification->set_fullscreen_visibility(
      message_center::FullscreenVisibility::OVER_USER);
  delegate_->ShowNotification(std::move(notification));
}

void OnTaskNotificationsManager::CreateToastInternal(
    ToastCreateParams& params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pending_notifications_map_.contains(params.id)) {
    // Stale toast. Ignore.
    return;
  }

  // Check if we've reached the countdown period and trigger callback if needed.
  if (params.countdown_period.is_zero() ||
      params.countdown_period.is_negative()) {
    params.completion_callback.Run();
    StopProcessingNotification(params.id);
    return;
  }

  // Display toast.
  ToastData toast_data(
      params.id, params.catalog_name,
      /*text=*/params.text_description_callback.Run(params.countdown_period));
  delegate_->ShowToast(std::move(toast_data));

  // Decrement countdown period before the next toast is scheduled.
  params.countdown_period =
      params.countdown_period - kOnTaskNotificationCountdownInterval;
}

}  // namespace ash::boca
