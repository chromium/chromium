// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_notification_manager.h"

#include "ash/public/cpp/notification_utils.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_dialog.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace ash::cloud_upload {
namespace {

// Minimum amount of time, in seconds, for which the notification should be
// displayed.
const base::TimeDelta kMinNotificationTime = base::Seconds(5);

// Time, in seconds, for which the "Complete" notification should display.
const base::TimeDelta kCompleteNotificationTimeout = base::Seconds(2);

// If no other class instance holds a reference to the notification manager, the
// notification manager goes out of scope.
void OnNotificationManagerDone(
    scoped_refptr<CloudUploadNotificationManager> notification_manager) {}

}  // namespace

CloudUploadNotificationManager::CloudUploadNotificationManager(
    Profile* profile,
    const std::string& file_name,
    const std::string& cloud_provider_name,
    const std::string& target_app_name)
    : profile_(profile),
      file_name_(file_name),
      cloud_provider_name_(cloud_provider_name),
      target_app_name_(target_app_name) {
  // Generate a unique ID for the cloud upload notifications.
  notification_id_ =
      "cloud-upload-" +
      base::NumberToString(
          ++CloudUploadNotificationManager::notification_manager_counter_);

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
  std::string title = "Moving \"" + file_name_ + "\"";
  std::string message = "Moving to " + cloud_provider_name_ +
                        ". Your file will open automatically when completed.";

  return ash::CreateSystemNotificationPtr(
      /*type=*/message_center::NOTIFICATION_TYPE_PROGRESS,
      /*id=*/notification_id_, base::UTF8ToUTF16(title),
      base::UTF8ToUTF16(message), /*display_source=*/std::u16string(),
      /*origin_url=*/GURL(), /*notifier_id=*/message_center::NotifierId(),
      /*optional_fields=*/{},
      /*delegate=*/
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(
              &CloudUploadNotificationManager::CloseNotification,
              weak_ptr_factory_.GetWeakPtr())),
      /*small_image=*/gfx::VectorIcon(),
      /*warning_level=*/message_center::SystemNotificationWarningLevel::NORMAL);
}

std::unique_ptr<message_center::Notification>
CloudUploadNotificationManager::CreateUploadCompleteNotification() {
  std::string title = "Move completed";
  std::string message =
      "1 item moved to \"from Chromebook\" folder. Opening in " +
      target_app_name_;
  return ash::CreateSystemNotificationPtr(
      /*type=*/message_center::NOTIFICATION_TYPE_SIMPLE,
      /*id=*/notification_id_, base::UTF8ToUTF16(title),
      base::UTF8ToUTF16(message),
      /*display_source=*/std::u16string(),
      /*origin_url=*/GURL(), /*notifier_id=*/message_center::NotifierId(),
      /*optional_fields=*/{},
      /*delegate=*/
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(
              &CloudUploadNotificationManager::CloseNotification,
              weak_ptr_factory_.GetWeakPtr())),
      /*small_image=*/gfx::VectorIcon(),
      /*warning_level=*/
      message_center::SystemNotificationWarningLevel::NORMAL);
}

std::unique_ptr<message_center::Notification>
CloudUploadNotificationManager::CreateUploadErrorNotification(
    std::string message) {
  std::string title = "Failed to move " + file_name_;
  return ash::CreateSystemNotificationPtr(
      /*type=*/message_center::NOTIFICATION_TYPE_SIMPLE,
      /*id=*/notification_id_, base::UTF8ToUTF16(title),
      base::UTF8ToUTF16(message),
      /*display_source=*/std::u16string(),
      /*origin_url=*/GURL(), /*notifier_id=*/message_center::NotifierId(),
      /*optional_fields=*/{},
      /*delegate=*/
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(
              &CloudUploadNotificationManager::CloseNotification,
              weak_ptr_factory_.GetWeakPtr())),
      /*small_image=*/gfx::VectorIcon(),
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

  // Start the "min time" notification timer when the first progress
  // notification is shown.
  if (!first_notification_shown) {
    first_notification_shown = true;
    notification_timer_.Start(
        FROM_HERE, kMinNotificationTime,
        base::BindOnce(
            &CloudUploadNotificationManager::OnMinNotificationTimeReached,
            weak_ptr_factory_.GetWeakPtr()));
  }
}

void CloudUploadNotificationManager::ShowUploadComplete() {
  std::unique_ptr<message_center::Notification> notification =
      CreateUploadCompleteNotification();
  notification->set_never_timeout(true);
  GetNotificationDisplayService()->Display(NotificationHandler::Type::TRANSIENT,
                                           *notification,
                                           /*metadata=*/nullptr);

  // If the complete notification is shown before any progress notifications,
  // start the `kMinNotificationTime` timer.
  if (!first_notification_shown) {
    first_notification_shown = true;
    notification_timer_.Start(
        FROM_HERE, kMinNotificationTime,
        base::BindOnce(
            &CloudUploadNotificationManager::OnMinNotificationTimeReached,
            weak_ptr_factory_.GetWeakPtr()));
  }

  // Start the timer to automatically dismiss the "Complete" notification.
  complete_notification_timer_.Start(
      FROM_HERE, kCompleteNotificationTimeout,
      base::BindOnce(
          &CloudUploadNotificationManager::OnCompleteNotificationTimeout,
          weak_ptr_factory_.GetWeakPtr()));
}

void CloudUploadNotificationManager::ShowUploadError(std::string message) {
  std::unique_ptr<message_center::Notification> notification =
      CreateUploadErrorNotification(message);
  notification->set_never_timeout(true);
  GetNotificationDisplayService()->Display(NotificationHandler::Type::TRANSIENT,
                                           *notification,
                                           /*metadata=*/nullptr);
}

void CloudUploadNotificationManager::OnMinNotificationTimeReached() {
  // Close the notification only if the "Complete notification" has timed out.
  // Error notifications can only be dismissed by users.
  if (completed_) {
    CloseNotification();
  }
}

void CloudUploadNotificationManager::OnCompleteNotificationTimeout() {
  completed_ = true;

  // If `kMinNotificationTime` hasn't been reached yet, do not close the
  // notification.
  if (!notification_timer_.IsRunning()) {
    CloseNotification();
  }
}

void CloudUploadNotificationManager::CloseNotification() {
  GetNotificationDisplayService()->Close(NotificationHandler::Type::TRANSIENT,
                                         notification_id_);
  notification_timer_.Stop();
  complete_notification_timer_.Stop();
  if (callback_) {
    std::move(callback_).Run();
  }
}

}  // namespace ash::cloud_upload
