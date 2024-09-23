// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/automated_mount_error_notification.h"

#include <memory>
#include <string>

#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/i18n/message_formatter.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_dialog.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash::cloud_upload {

namespace {

constexpr char kAutomatedMountErrorNotificationId[] =
    "automated_mount_error_notification_id";

void HandleErrorNotificationClick(base::WeakPtr<Profile> profile,
                                  std::optional<int> button_index) {
  if (!profile) {
    return;
  }

  // If the "Sign in" button was pressed, rather than a click to somewhere
  // else in the notification.
  if (button_index) {
    ash::cloud_upload::ShowConnectOneDriveDialog(/*modal_parent=*/nullptr);
  }

  NotificationDisplayServiceFactory::GetForProfile(profile.get())
      ->Close(NotificationHandler::Type::TRANSIENT,
              kAutomatedMountErrorNotificationId);
}

std::unique_ptr<message_center::Notification>
CreateAutomatedMountErrorNotification(Profile& profile) {
  auto notification = ash::CreateSystemNotificationPtr(
      /*type=*/message_center::NOTIFICATION_TYPE_SIMPLE,
      /*id=*/kAutomatedMountErrorNotificationId,
      /*title=*/
      l10n_util::GetStringUTF16(IDS_ONEDRIVE_AUTOMATED_MOUNT_ERROR_TITLE),
      /*message=*/
      l10n_util::GetStringUTF16(IDS_ONEDRIVE_AUTOMATED_MOUNT_ERROR_MESSAGE),
      /*display_source=*/
      l10n_util::GetStringUTF16(IDS_ASH_MESSAGE_CENTER_SYSTEM_APP_NAME_FILES),
      /*origin_url=*/GURL(), /*notifier_id=*/message_center::NotifierId(),
      /*optional_fields=*/{},
      /*delegate=*/
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(HandleErrorNotificationClick,
                              profile.GetWeakPtr())),
      /*small_image=*/ash::kFolderIcon,
      /*warning_level=*/
      message_center::SystemNotificationWarningLevel::WARNING);

  std::u16string button_title =
      l10n_util::GetStringUTF16(IDS_ONEDRIVE_AUTOMATED_MOUNT_BUTTON_TITLE);
  std::vector<message_center::ButtonInfo> notification_buttons;
  notification_buttons.emplace_back(button_title);
  notification->set_buttons(notification_buttons);

  return notification;
}

}  // namespace

void ShowAutomatedMountErrorNotification(Profile& profile) {
  std::unique_ptr<message_center::Notification> notification =
      CreateAutomatedMountErrorNotification(profile);
  // Set never_timeout with the highest priority, SYSTEM_PRIORITY, so that the
  // notification never times out.
  notification->set_never_timeout(true);
  notification->SetSystemPriority();
  NotificationDisplayServiceFactory::GetForProfile(&profile)->Display(
      NotificationHandler::Type::TRANSIENT, *notification,
      /*metadata=*/nullptr);
}

}  // namespace ash::cloud_upload
