// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_CLOUD_UPLOAD_CLOUD_UPLOAD_NOTIFICATION_MANAGER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_CLOUD_UPLOAD_CLOUD_UPLOAD_NOTIFICATION_MANAGER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "ui/message_center/public/cpp/notification.h"

class Profile;

namespace chromeos::cloud_upload {

// Manages creation/deletion and update of cloud upload system notifications.
class CloudUploadNotificationManager {
 public:
  explicit CloudUploadNotificationManager(Profile* profile);
  ~CloudUploadNotificationManager();

  // Shows the upload progress notification (progress within the 0-100 range).
  void ShowProgress(int progress);

  // Dismisses the notification.
  void Dismiss();

 private:
  // Returns the message center display service that manages notifications.
  NotificationDisplayService* GetNotificationDisplayService();

  // Returns an instance of an 'ash' progress notification.
  std::unique_ptr<message_center::Notification> CreateProgressNotification();

  Profile* const profile_;
  base::WeakPtrFactory<CloudUploadNotificationManager> weak_ptr_factory_{this};
};

}  // namespace chromeos::cloud_upload

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_CLOUD_UPLOAD_CLOUD_UPLOAD_NOTIFICATION_MANAGER_H_
