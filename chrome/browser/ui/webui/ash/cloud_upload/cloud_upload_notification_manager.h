// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_UPLOAD_NOTIFICATION_MANAGER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_UPLOAD_NOTIFICATION_MANAGER_H_

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_dialog.h"
#include "ui/message_center/public/cpp/notification.h"

class Profile;

namespace ash::cloud_upload {

// Manages creation/deletion and update of cloud upload system notifications.
class CloudUploadNotificationManager
    : public base::RefCounted<CloudUploadNotificationManager> {
 public:
  CloudUploadNotificationManager(Profile* profile,
                                 const std::string& file_name,
                                 const std::string& cloud_provider_name,
                                 const std::string& target_app_name);

  // Shows the upload progress notification. |progress| within the 0-100 range.
  void ShowUploadProgress(int progress);

  // Shows the upload complete notification for 2s if the upload was successful.
  void ShowUploadComplete();

  // Shows the upload error notification.
  void ShowUploadError(std::string message);

 private:
  friend base::RefCounted<CloudUploadNotificationManager>;
  ~CloudUploadNotificationManager();

  // Returns the message center display service that manages notifications.
  NotificationDisplayService* GetNotificationDisplayService();

  // Returns an instance of an 'ash' upload progress notification.
  std::unique_ptr<message_center::Notification>
  CreateUploadProgressNotification();

  // Returns an instance of an 'ash' upload complete notification.
  std::unique_ptr<message_center::Notification>
  CreateUploadCompleteNotification();

  // Returns an instance of an 'ash' upload error notification.
  std::unique_ptr<message_center::Notification> CreateUploadErrorNotification(
      std::string message);

  // Called when the minimum amount of time to display the notification is
  // reached.
  void OnMinNotificationTimeReached();

  // Called when the "Complete" notification times out.
  void OnCompleteNotificationTimeout();

  // Called when the upload flow is complete: Ensures that notifications are
  // closed, timers are interrupted and the completion callback has been called.
  void CloseNotification();

  // Counts the total number of notification manager instances. This counter is
  // never decremented.
  static inline int notification_manager_counter_ = 0;

  Profile* const profile_;
  CloudProvider provider_;
  std::string file_name_;
  std::string cloud_provider_name_;
  std::string notification_id_;
  std::string target_app_name_;
  base::OnceClosure callback_;
  base::OneShotTimer notification_timer_;
  base::OneShotTimer complete_notification_timer_;
  bool first_notification_shown = false;
  bool completed_ = false;
  base::WeakPtrFactory<CloudUploadNotificationManager> weak_ptr_factory_{this};
};

}  // namespace ash::cloud_upload

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_UPLOAD_NOTIFICATION_MANAGER_H_
