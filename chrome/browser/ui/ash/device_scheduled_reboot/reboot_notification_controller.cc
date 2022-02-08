// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/device_scheduled_reboot/reboot_notification_controller.h"

#include <memory>

#include "ash/public/cpp/notification_utils.h"
#include "base/callback_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/memory/ptr_util.h"
#include "base/strings/strcat.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/user_manager.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"

namespace {

// Id of the pending reboot notification
constexpr char kPendingRebootNotificationId[] =
    "ash.device_scheduled_reboot_pending_notification";

}  // namespace

RebootNotificationController::RebootNotificationController() = default;

RebootNotificationController::~RebootNotificationController() = default;

void RebootNotificationController::MaybeShowPendingRebootNotification(
    const base::Time& reboot_time,
    ButtonClickCallback reboot_callback) const {
  if (!user_manager::UserManager::IsInitialized() ||
      !user_manager::UserManager::Get()->IsUserLoggedIn() ||
      user_manager::UserManager::Get()->IsLoggedInAsAnyKioskApp()) {
    return;
  }

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
          std::move(reboot_callback));

  ShowNotification(kPendingRebootNotificationId, reboot_title, reboot_message,
                   notification_data, delegate);
}

void RebootNotificationController::ShowNotification(
    const std::string& id,
    const std::u16string& title,
    const std::u16string& message,
    const message_center::RichNotificationData& data,
    scoped_refptr<message_center::NotificationDelegate> delegate) const {
  // Create notification.
  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE, id, title, message,
          std::u16string(), GURL(), message_center::NotifierId(), data,
          delegate, vector_icons::kBusinessIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);

  NotificationDisplayService* notification_display_service =
      NotificationDisplayService::GetForProfile(
          ProfileManager::GetActiveUserProfile());
  // Close old notification.
  notification_display_service->Close(NotificationHandler::Type::TRANSIENT, id);
  // Display new notification.
  notification_display_service->Display(NotificationHandler::Type::TRANSIENT,
                                        *notification,
                                        /*metadata=*/nullptr);
}