// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/device_scheduled_reboot/reboot_notification_controller.h"

#include <memory>

#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "base/check_deref.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/memory/ptr_util.h"
#include "base/strings/strcat.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

// Id of the pending reboot notification
const char kPendingRebootNotificationId[] =
    "ash.device_scheduled_reboot_pending_notification";

// Id of the post reboot notification
const char kPostRebootNotificationId[] =
    "ash.device_scheduled_reboot_post_reboot_notification";
}  // namespace ash

namespace {

// Returns the user the notifications belong to. The callers have established
// through ShouldNotifyUser() that somebody is logged in, and a logged-in user
// always has a session.
const user_manager::User& GetActiveUser() {
  const session_manager::Session& session =
      CHECK_DEREF(session_manager::SessionManager::Get()->GetActiveSession());
  return CHECK_DEREF(
      user_manager::UserManager::Get()->FindUser(session.account_id()));
}

}  // namespace

RebootNotificationController::RebootNotificationController() = default;

RebootNotificationController::~RebootNotificationController() = default;

void RebootNotificationController::MaybeShowPendingRebootNotification(
    const base::Time& reboot_time,
    base::RepeatingClosure reboot_callback) {
  if (!ShouldNotifyUser()) {
    return;
  }
  notification_callback_ = std::move(reboot_callback);
  std::u16string reboot_title =
      l10n_util::GetStringUTF16(IDS_POLICY_DEVICE_SCHEDULED_REBOOT_TITLE);
  std::u16string reboot_message =
      l10n_util::GetStringFUTF16(IDS_POLICY_DEVICE_SCHEDULED_REBOOT_MESSAGE,
                                 base::TimeFormatTimeOfDay(reboot_time),
                                 base::TimeFormatShortDate(reboot_time));

  message_center::RichNotificationData notification_data;
  notification_data.pinned = true;
  notification_data.buttons.emplace_back(
      l10n_util::GetStringUTF16(IDS_POLICY_REBOOT_BUTTON));
  scoped_refptr<message_center::NotificationDelegate> delegate =
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(
              &RebootNotificationController::HandleNotificationClick,
              weak_ptr_factory_.GetWeakPtr()));

  ShowNotification(ash::kPendingRebootNotificationId, reboot_title,
                   reboot_message, notification_data, delegate);
}

void RebootNotificationController::MaybeShowPendingRebootDialog(
    const base::Time& reboot_time,
    base::OnceClosure reboot_callback) {
  if (!ShouldNotifyUser()) {
    return;
  }

  gfx::NativeView parent =
      ash::Shell::GetContainer(ash::Shell::GetRootWindowForNewWindows(),
                               ash::kShellWindowId_SystemModalContainer);
  // Closes old dialog if it was active at the moment and shows a new dialog
  // notifying the user about the reboot.
  scheduled_reboot_dialog_ = std::make_unique<ScheduledRebootDialog>(
      reboot_time, parent, std::move(reboot_callback));
}

void RebootNotificationController::MaybeShowPostRebootNotification() const {
  if (!ShouldNotifyUser()) {
    return;
  }
  std::u16string title =
      l10n_util::GetStringUTF16(IDS_POLICY_DEVICE_POST_REBOOT_TITLE);
  scoped_refptr<message_center::NotificationDelegate> delegate =
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(
              &RebootNotificationController::HandleNotificationClick,
              weak_ptr_factory_.GetWeakPtr()));
  ShowNotification(ash::kPostRebootNotificationId, title, std::u16string(),
                   message_center::RichNotificationData(), delegate);
}

void RebootNotificationController::CloseRebootNotification() const {
  if (!ShouldNotifyUser()) {
    return;
  }
  message_center::MessageCenter::Get()->RemoveNotification(
      ash::CreateUserScopedNotificationId(ash::kPendingRebootNotificationId,
                                          GetActiveUser().username_hash()),
      /*by_user=*/false);
}

void RebootNotificationController::CloseRebootDialog() {
  if (scheduled_reboot_dialog_) {
    scheduled_reboot_dialog_.reset();
  }
}

void RebootNotificationController::ShowNotification(
    const std::string& id,
    const std::u16string& title,
    const std::u16string& message,
    const message_center::RichNotificationData& data,
    scoped_refptr<message_center::NotificationDelegate> delegate) const {
  // Create notification.
  const user_manager::User& user = GetActiveUser();
  const std::string notification_id =
      ash::CreateUserScopedNotificationId(id, user.username_hash());
  message_center::NotifierId notifier_id;
  notifier_id.profile_id = user.GetAccountId().GetUserEmail();
  auto notification = ash::CreateSystemNotificationPtr(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id, title, message,
      std::u16string(), notifier_id, data, delegate,
      ::features::IsRoundedIconsEnabled() ? vector_icons::kDomainIcon
                                          : vector_icons::kBusinessOldIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);

  // Close old notification.
  message_center::MessageCenter::Get()->RemoveNotification(notification_id,
                                                           /*by_user=*/false);
  // Display new notification.
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

bool RebootNotificationController::ShouldNotifyUser() const {
  return (user_manager::UserManager::IsInitialized() &&
          user_manager::UserManager::Get()->IsUserLoggedIn() &&
          !user_manager::UserManager::Get()->IsLoggedInAsAnyKioskApp());
}

void RebootNotificationController::HandleNotificationClick(
    std::optional<int> button_index) const {
  // Only request restart when the button is clicked, i.e. ignore the clicks
  // on the body of the notification.
  if (!button_index) {
    return;
  }
  notification_callback_.Run();
}
