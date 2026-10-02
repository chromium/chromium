// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/automated_mount_error_notification.h"

#include <memory>
#include <string>

#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/functional/bind.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_dialog.h"
#include "chrome/grit/generated_resources.h"
#include "components/user_manager/user.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash::cloud_upload {

namespace {

constexpr char kAutomatedMountErrorNotificationId[] =
    "automated_mount_error_notification_id";

void HandleErrorNotificationClick(const std::string& notification_id,
                                  std::optional<int> button_index) {
  message_center::MessageCenter::Get()->RemoveNotification(notification_id,
                                                           /*by_user=*/false);

  // If the "Sign in" button was pressed, rather than a click to somewhere
  // else in the notification.
  if (button_index) {
    ash::cloud_upload::ShowConnectOneDriveDialog(/*modal_parent=*/nullptr);
  }
}

std::unique_ptr<message_center::Notification>
CreateAutomatedMountErrorNotification(const user_manager::User& user) {
  const std::string notification_id = ash::CreateUserScopedNotificationId(
      kAutomatedMountErrorNotificationId, user.username_hash());

  // Set `profile_id` so that the notification is hidden while another
  // signed-in user is active.
  message_center::NotifierId notifier_id;
  notifier_id.profile_id = user.GetAccountId().GetUserEmail();

  message_center::RichNotificationData optional_fields;
  optional_fields.buttons.emplace_back(
      l10n_util::GetStringUTF16(IDS_ONEDRIVE_AUTOMATED_MOUNT_BUTTON_TITLE));

  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotificationPtr(
          /*type=*/message_center::NOTIFICATION_TYPE_SIMPLE,
          /*id=*/notification_id,
          /*title=*/
          l10n_util::GetStringUTF16(IDS_ONEDRIVE_AUTOMATED_MOUNT_ERROR_TITLE),
          /*message=*/
          l10n_util::GetStringUTF16(IDS_ONEDRIVE_AUTOMATED_MOUNT_ERROR_MESSAGE),
          /*display_source=*/
          l10n_util::GetStringUTF16(
              IDS_ASH_MESSAGE_CENTER_SYSTEM_APP_NAME_FILES),
          /*notifier_id=*/notifier_id,
          /*optional_fields=*/optional_fields,
          /*delegate=*/
          base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
              base::BindRepeating(HandleErrorNotificationClick,
                                  notification_id)),
          /*small_image=*/ash::kFolderIcon,
          /*warning_level=*/
          message_center::SystemNotificationWarningLevel::WARNING);
  notification->SetSystemPriority();
  return notification;
}

}  // namespace

void ShowAutomatedMountErrorNotification(const user_manager::User& user) {
  message_center::MessageCenter::Get()->AddNotification(
      CreateAutomatedMountErrorNotification(user));
}

}  // namespace ash::cloud_upload
