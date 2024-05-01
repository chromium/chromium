// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_UPLOAD_NOTIFICATION_MANAGER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_UPLOAD_NOTIFICATION_MANAGER_H_

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "ui/message_center/public/cpp/notification.h"

class Profile;

namespace ash::cloud_upload {

// Creates, updates and deletes cloud upload system notifications. Ensures that
// notifications stay in the "in progress" state for a minimum of 5 seconds, and
// a minimum of 5 seconds for the 'complete' state unless the user chooses to
// click the "Show in folder" button which would close the notification early.
// For the error state, notifications stay open until the user closes them.
class CloudUploadNotificationManager
    : public base::RefCounted<CloudUploadNotificationManager> {
 public:
  using HandleCompleteNotificationClickCallback =
      base::OnceCallback<void(base::FilePath)>;

  CloudUploadNotificationManager(Profile* profile,
                                 const std::string& cloud_provider_name,
                                 const std::string& target_app_name,
                                 int num_files,
                                 UploadType upload_type);

  // Creates the notification with "in progress" state if it doesn't exist, or
  // updates the progress bar if it does. |progress| is within the 0-100 range.
  // The notification will stay in the "in progress" state for a minimum of 5
  // seconds, even at 100% progress.
  void ShowUploadProgress(int progress);

  // Shows the upload complete notification for 5 seconds, but only once the
  // minimum 5 seconds from the "in progress" state has finished.
  void MarkUploadComplete();

  // Shows the error state for the notification indefinitely, until closed by
  // the user. Does not wait for the progress notification to show for a minimum
  // time.
  void ShowUploadError(const std::string& message);

  // Closes any existing notification, interrupts ongoing timers, and calls the
  // completion callback, destroying this instance. This is called internally
  // after the notification has completed and timed out, or when the error
  // notification is clicked. It only needs to be called publicly in tests to
  // avoid leaking instances of this class, or when no notification was ever
  // started.
  void CloseNotification();

  void SetDestinationPath(base::FilePath destination_path) {
    destination_path_ = destination_path;
  }

  void SetCancelCallback(base::OnceClosure cancel_callback) {
    cancel_callback_ = std::move(cancel_callback);
  }

  // Used in tests to set a callback to check if
  // |HandleCompleteNotificationClick| is called with the expected
  // |destination_path_|.
  void SetHandleCompleteNotificationClickCallbackForTesting(
      HandleCompleteNotificationClickCallback callback) {
    callback_for_testing_ = std::move(callback);
  }

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

  // Called when the minimum amount of time to display the "in progress"
  // notification is reached.
  void OnMinInProgressTimeReached();

  // Updates the notification immediately to show the complete state.
  void ShowCompleteNotification();

  // "Cancel" click handler for upload progress notification.
  void HandleProgressNotificationClick(std::optional<int> button_index);

  // "Sign in" click handler for authentication error notification.
  void HandleErrorNotificationClick(std::optional<int> button_index);

  // "Show in folder" click handler for upload complete notification.
  void HandleCompleteNotificationClick(std::optional<int> button_index);

  // A state machine and the possible transitions. The state of showing the
  // error notification is not explicit because it is never used to determine
  // later logic.
  enum class State {
    kUninitialized,  // --> kInProgress, kComplete
    kInProgress,     // --> kInProgressTimedOut, kWaitingForInProgressTimeout,
                     // (error)
    kInProgressTimedOut,           // --> kComplete, (error)
    kWaitingForInProgressTimeout,  // --> kComplete
    kComplete
  };

  // Returns true if upload is still in progress.
  bool CanCancel();

  // Counts the total number of notification manager instances. This counter is
  // never decremented.
  static inline int notification_manager_counter_ = 0;

  const raw_ptr<Profile, LeakedDanglingUntriaged> profile_;
  CloudProvider provider_;
  std::string cloud_provider_name_;
  std::string notification_id_;
  std::string target_app_name_;
  std::u16string display_source_;
  int num_files_;
  int progress_;
  UploadType upload_type_;
  base::FilePath destination_path_;
  base::OnceClosure callback_;
  base::OnceClosure cancel_callback_;
  HandleCompleteNotificationClickCallback callback_for_testing_;
  base::OneShotTimer in_progress_timer_;
  base::OneShotTimer complete_notification_timer_;
  State state_ = State::kUninitialized;
  base::WeakPtrFactory<CloudUploadNotificationManager> weak_ptr_factory_{this};
};

}  // namespace ash::cloud_upload

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_UPLOAD_NOTIFICATION_MANAGER_H_
