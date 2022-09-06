// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/cloud_upload/cloud_upload_notification_manager.h"

#include "ash/public/cpp/notification_utils.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace chromeos::cloud_upload {
namespace {

constexpr char kCloudUploadProgressNotificationId[] = "cloud-upload-progress";

}  // namespace

CloudUploadNotificationManager::CloudUploadNotificationManager(Profile* profile)
    : profile_(profile) {}

CloudUploadNotificationManager::~CloudUploadNotificationManager() {
  // Make sure open notifications are dismissed before the notification manager
  // goes out of scope.
  // TODO(b/242685213) Make sure error notifications are not dismissed until
  // user closes them.
  Dismiss();
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
          base::BindRepeating(&CloudUploadNotificationManager::Dismiss,
                              weak_ptr_factory_.GetWeakPtr())),
      /*small_image=*/gfx::VectorIcon(),
      /*warning_level=*/message_center::SystemNotificationWarningLevel::NORMAL);
}

void CloudUploadNotificationManager::ShowProgress(int progress) {
  std::unique_ptr<message_center::Notification> notification =
      CreateProgressNotification();
  notification->set_progress(progress);
  GetNotificationDisplayService()->Display(NotificationHandler::Type::TRANSIENT,
                                           *notification,
                                           /*metadata=*/nullptr);
}

void CloudUploadNotificationManager::Dismiss() {
  GetNotificationDisplayService()->Close(NotificationHandler::Type::TRANSIENT,
                                         kCloudUploadProgressNotificationId);
}

}  // namespace chromeos::cloud_upload
