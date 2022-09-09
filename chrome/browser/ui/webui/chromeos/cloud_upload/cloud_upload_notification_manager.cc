// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/cloud_upload/cloud_upload_notification_manager.h"

#include "ash/public/cpp/notification_utils.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace chromeos::cloud_upload {
namespace {

constexpr char kCloudUploadProgressNotificationId[] = "cloud-upload-progress";

// If no other class instance holds a reference to the notification manager, the
// notification manager goes out of scope.
void OnNotificationManagerDone(
    scoped_refptr<CloudUploadNotificationManager> notification_manager) {}

}  // namespace

CloudUploadNotificationManager::CloudUploadNotificationManager(Profile* profile)
    : profile_(profile) {
  // Keep the new `CloudUploadNotificationManager` instance alive at least until
  // `OnNotificationManagerDone` executes.
  callback_ =
      base::BindOnce(&OnNotificationManagerDone, base::WrapRefCounted(this));
}

CloudUploadNotificationManager::~CloudUploadNotificationManager() {
  // Make sure open notifications are dismissed before the notification manager
  // goes out of scope.
  GetNotificationDisplayService()->Close(NotificationHandler::Type::TRANSIENT,
                                         kCloudUploadProgressNotificationId);
}

NotificationDisplayService*
CloudUploadNotificationManager::GetNotificationDisplayService() {
  return NotificationDisplayServiceFactory::GetForProfile(profile_);
}

std::unique_ptr<message_center::Notification>
CloudUploadNotificationManager::CreateProgressNotification() {
  return ash::CreateSystemNotification(
      /*type=*/message_center::NOTIFICATION_TYPE_PROGRESS,
      /*id=*/kCloudUploadProgressNotificationId,
      /*title=*/std::u16string(u"Office upload"),
      /*message=*/std::u16string(), /*display_source=*/std::u16string(),
      /*origin_url=*/GURL(), /*notifier_id=*/message_center::NotifierId(),
      /*optional_fields=*/{},
      /*delegate=*/
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(&CloudUploadNotificationManager::Completed,
                              weak_ptr_factory_.GetWeakPtr())),
      /*small_image=*/gfx::VectorIcon(),
      /*warning_level=*/message_center::SystemNotificationWarningLevel::NORMAL);
}

std::unique_ptr<message_center::Notification>
CloudUploadNotificationManager::CreateErrorNotification(std::string message) {
  return ash::CreateSystemNotification(
      /*type=*/message_center::NOTIFICATION_TYPE_SIMPLE,
      /*id=*/kCloudUploadProgressNotificationId,
      /*title=*/std::u16string(u"Office upload error"),
      /*message=*/base::UTF8ToUTF16(message),
      /*display_source=*/std::u16string(),
      /*origin_url=*/GURL(), /*notifier_id=*/message_center::NotifierId(),
      /*optional_fields=*/{},
      /*delegate=*/
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(&CloudUploadNotificationManager::Completed,
                              weak_ptr_factory_.GetWeakPtr())),
      /*small_image=*/gfx::VectorIcon(),
      /*warning_level=*/
      message_center::SystemNotificationWarningLevel::WARNING);
}

void CloudUploadNotificationManager::ShowProgress(int progress) {
  std::unique_ptr<message_center::Notification> notification =
      CreateProgressNotification();
  notification->set_progress(progress);
  notification->set_never_timeout(true);
  GetNotificationDisplayService()->Display(NotificationHandler::Type::TRANSIENT,
                                           *notification,
                                           /*metadata=*/nullptr);
}

void CloudUploadNotificationManager::ShowError(std::string message) {
  std::unique_ptr<message_center::Notification> notification =
      CreateErrorNotification(message);
  notification->set_never_timeout(true);
  GetNotificationDisplayService()->Display(NotificationHandler::Type::TRANSIENT,
                                           *notification,
                                           /*metadata=*/nullptr);
}

void CloudUploadNotificationManager::Completed() {
  GetNotificationDisplayService()->Close(NotificationHandler::Type::TRANSIENT,
                                         kCloudUploadProgressNotificationId);
  if (callback_) {
    std::move(callback_).Run();
  }
}

}  // namespace chromeos::cloud_upload
