// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_notification_manager.h"

#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_dialog.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace ash::cloud_upload {
namespace {

// The minimum amount of time for which the "in progress" state should be
// displayed.
const base::TimeDelta kMinInProgressTime = base::Seconds(5);

// Time for which the "Complete" notification should display.
const base::TimeDelta kCompleteNotificationTime = base::Seconds(5);

// If no other class instance holds a reference to the notification manager, the
// notification manager goes out of scope.
void OnNotificationManagerDone(
    scoped_refptr<CloudUploadNotificationManager> notification_manager) {}

}  // namespace

CloudUploadNotificationManager::CloudUploadNotificationManager(
    Profile* profile,
    const std::string& file_name,
    const std::string& cloud_provider_name,
    const std::string& target_app_name,
    int num_files)
    : profile_(profile),
      file_name_(file_name),
      cloud_provider_name_(cloud_provider_name),
      target_app_name_(target_app_name),
      num_files_(num_files) {
  // Generate a unique ID for the cloud upload notifications.
  notification_id_ =
      "cloud-upload-" +
      base::NumberToString(
          ++CloudUploadNotificationManager::notification_manager_counter_);

  // Set the system notification source display name to "Files".
  display_source_ =
      l10n_util::GetStringUTF16(IDS_ASH_MESSAGE_CENTER_SYSTEM_APP_NAME_FILES);

  // Keep the new `CloudUploadNotificationManager` instance alive at least until
  // `OnNotificationManagerDone` executes.
  callback_ =
      base::BindOnce(&OnNotificationManagerDone, base::WrapRefCounted(this));
}

CloudUploadNotificationManager::~CloudUploadNotificationManager() {
  // Make sure open notifications are dismissed before the notification manager
  // goes out of scope.
  CloseNotification();
}

NotificationDisplayService*
CloudUploadNotificationManager::GetNotificationDisplayService() {
  return NotificationDisplayServiceFactory::GetForProfile(profile_);
}

std::unique_ptr<message_center::Notification>
CloudUploadNotificationManager::CreateUploadProgressNotification() {
  std::string title =
      // TODO(b/242685536) Use "files" for multi-files when support for
      // multi-files is added.
      "Moving " + base::NumberToString(num_files_) + " file to " +
      cloud_provider_name_;
  std::string message = "File will open in " + target_app_name_;

  return ash::CreateSystemNotificationPtr(
      /*type=*/message_center::NOTIFICATION_TYPE_PROGRESS,
      /*id=*/notification_id_, base::UTF8ToUTF16(title),
      // TODO(b/272601262) Display or delete this message.
      base::UTF8ToUTF16(message), /*display_source=*/display_source_,
      /*origin_url=*/GURL(), /*notifier_id=*/message_center::NotifierId(),
      /*optional_fields=*/{},
      /*delegate=*/
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(
              &CloudUploadNotificationManager::CloseNotification,
              weak_ptr_factory_.GetWeakPtr())),
      /*small_image=*/ash::kFolderIcon,
      /*warning_level=*/message_center::SystemNotificationWarningLevel::NORMAL);
}

std::unique_ptr<message_center::Notification>
CloudUploadNotificationManager::CreateUploadCompleteNotification() {
  // TODO(b/242685536) Use "files" for multi-files when support for multi-files
  // is added.
  std::string title = "Moved " + base::NumberToString(num_files_) +
                      " file to " + cloud_provider_name_;
  std::string message = "Opening in " + target_app_name_;
  auto notification = ash::CreateSystemNotificationPtr(
      /*type=*/message_center::NOTIFICATION_TYPE_SIMPLE,
      /*id=*/notification_id_, base::UTF8ToUTF16(title),
      base::UTF8ToUTF16(message),
      /*display_source=*/display_source_,
      /*origin_url=*/GURL(), /*notifier_id=*/message_center::NotifierId(),
      /*optional_fields=*/{},
      /*delegate=*/
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(
              &CloudUploadNotificationManager::HandleNotificationClick,
              weak_ptr_factory_.GetWeakPtr())),
      /*small_image=*/ash::kFolderIcon,
      /*warning_level=*/
      message_center::SystemNotificationWarningLevel::NORMAL);

  DCHECK(!destination_path_.empty());
  if (!destination_path_.empty()) {
    //  Add "Show in folder" button.
    std::string button_title = "Show in folder";
    std::vector<message_center::ButtonInfo> notification_buttons = {
        message_center::ButtonInfo(base::UTF8ToUTF16(button_title))};
    notification->set_buttons(notification_buttons);
  }
  return notification;
}

std::unique_ptr<message_center::Notification>
CloudUploadNotificationManager::CreateUploadErrorNotification(
    std::string message) {
  std::string title = "Can't move file";
  return ash::CreateSystemNotificationPtr(
      /*type=*/message_center::NOTIFICATION_TYPE_SIMPLE,
      /*id=*/notification_id_, base::UTF8ToUTF16(title),
      base::UTF8ToUTF16(message),
      /*display_source=*/display_source_,
      /*origin_url=*/GURL(), /*notifier_id=*/message_center::NotifierId(),
      /*optional_fields=*/{},
      /*delegate=*/
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(
              &CloudUploadNotificationManager::CloseNotification,
              weak_ptr_factory_.GetWeakPtr())),
      /*small_image=*/ash::kFolderIcon,
      /*warning_level=*/
      message_center::SystemNotificationWarningLevel::WARNING);
}

void CloudUploadNotificationManager::ShowUploadProgress(int progress) {
  std::unique_ptr<message_center::Notification> notification =
      CreateUploadProgressNotification();
  notification->set_progress(progress);
  notification->set_never_timeout(true);
  GetNotificationDisplayService()->Display(NotificationHandler::Type::TRANSIENT,
                                           *notification,
                                           /*metadata=*/nullptr);

  // Make sure we display the "in progress" state for a minimum amount of time.
  if (state_ == State::kUninitialized) {
    state_ = State::kInProgress;
    in_progress_timer_.Start(
        FROM_HERE, kMinInProgressTime,
        base::BindOnce(
            &CloudUploadNotificationManager::OnMinInProgressTimeReached,
            weak_ptr_factory_.GetWeakPtr()));
  }
}

void CloudUploadNotificationManager::ShowCompleteNotification() {
  DCHECK_EQ(state_, State::kComplete);
  std::unique_ptr<message_center::Notification> notification =
      CreateUploadCompleteNotification();
  notification->set_never_timeout(true);
  GetNotificationDisplayService()->Display(NotificationHandler::Type::TRANSIENT,
                                           *notification,
                                           /*metadata=*/nullptr);

  // Start the timer to automatically dismiss the "Complete" notification if
  // "Show in folder" button not clicked.
  complete_notification_timer_.Start(
      FROM_HERE, kCompleteNotificationTime,
      base::BindOnce(&CloudUploadNotificationManager::CloseNotification,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CloudUploadNotificationManager::MarkUploadComplete() {
  // Check if the "in progress" timeout has happened yet or not.
  if (state_ == State::kInProgress) {
    state_ = State::kWaitingForInProgressTimeout;
  } else if (state_ == State::kUninitialized ||
             state_ == State::kInProgressTimedOut) {
    // If the complete notification is shown before any progress notifications,
    // we don't run the kMinInProgressTime timeout.
    state_ = State::kComplete;
    ShowCompleteNotification();
  }
}

void CloudUploadNotificationManager::ShowUploadError(
    const std::string& message) {
  std::unique_ptr<message_center::Notification> notification =
      CreateUploadErrorNotification(message);
  notification->set_never_timeout(true);
  GetNotificationDisplayService()->Display(NotificationHandler::Type::TRANSIENT,
                                           *notification,
                                           /*metadata=*/nullptr);
}

void CloudUploadNotificationManager::OnMinInProgressTimeReached() {
  if (state_ == State::kInProgress) {
    state_ = State::kInProgressTimedOut;
  } else if (state_ == State::kWaitingForInProgressTimeout) {
    state_ = State::kComplete;
    ShowCompleteNotification();
  }
}

void CloudUploadNotificationManager::CloseNotification() {
  GetNotificationDisplayService()->Close(NotificationHandler::Type::TRANSIENT,
                                         notification_id_);
  in_progress_timer_.Stop();
  complete_notification_timer_.Stop();
  if (callback_) {
    std::move(callback_).Run();
  }
}

void CloudUploadNotificationManager::HandleNotificationClick(
    absl::optional<int> button_index) {
  if (callback_for_testing_) {
    std::move(callback_for_testing_).Run(destination_path_);
  } else if (button_index) {
    // Show In Folder if button was pressed, rather than a click to somewhere
    // else in the notification.
    platform_util::ShowItemInFolder(profile_, destination_path_);
  }

  CloseNotification();
}

void CloudUploadNotificationManager::CloseForTest() {
  CloseNotification();
}

}  // namespace ash::cloud_upload
