// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_notification_manager.h"

#include <optional>

#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/stub_notification_display_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"

namespace ash::cloud_upload {

class CloudUploadNotificationManagerTest : public testing::Test {
 public:
  CloudUploadNotificationManagerTest() = default;

  CloudUploadNotificationManagerTest(
      const CloudUploadNotificationManagerTest&) = delete;
  CloudUploadNotificationManagerTest& operator=(
      const CloudUploadNotificationManagerTest&) = delete;

  // testing::Test:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();

    display_service_ = static_cast<StubNotificationDisplayService*>(
        NotificationDisplayServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                profile_.get(),
                base::BindRepeating(
                    &StubNotificationDisplayService::FactoryForTests)));
  }

 protected:
  Profile* profile() { return profile_.get(); }

  std::optional<message_center::Notification> notification() {
    auto notifications = display_service_->GetDisplayedNotificationsForType(
        NotificationHandler::Type::TRANSIENT);
    if (notifications.size()) {
      return notifications[0];
    }
    return std::nullopt;
  }

  bool HaveMoveProgressNotification() {
    return notification().has_value() &&
           notification()->type() ==
               message_center::NotificationType::NOTIFICATION_TYPE_PROGRESS &&
           notification()->title().starts_with(u"Moving ") &&
           notification()->display_source() ==
               l10n_util::GetStringUTF16(
                   IDS_ASH_MESSAGE_CENTER_SYSTEM_APP_NAME_FILES);
  }

  bool HaveMoveProgressNotificationWithCancelButton() {
    return HaveMoveProgressNotification() &&
           !notification()->buttons().empty() &&
           (notification()->buttons().front().title == u"Cancel");
  }

  bool HaveMoveProgressNotificationWithoutCancelButton() {
    return HaveMoveProgressNotification() && notification()->buttons().empty();
  }

  bool HaveCopyProgressNotification() {
    return notification().has_value() &&
           notification()->type() ==
               message_center::NotificationType::NOTIFICATION_TYPE_PROGRESS &&
           notification()->title().starts_with(u"Copying ") &&
           notification()->display_source() ==
               l10n_util::GetStringUTF16(
                   IDS_ASH_MESSAGE_CENTER_SYSTEM_APP_NAME_FILES);
  }

  bool HaveMoveCompleteNotification() {
    return notification().has_value() &&
           notification()->type() ==
               message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE &&
           notification()->title().starts_with(u"Moved ") &&
           notification()->display_source() ==
               l10n_util::GetStringUTF16(
                   IDS_ASH_MESSAGE_CENTER_SYSTEM_APP_NAME_FILES) &&
           !notification()->buttons().empty() &&
           (notification()->buttons().front().title == u"Show in folder");
  }

  bool HaveCopyCompleteNotification() {
    return notification().has_value() &&
           notification()->type() ==
               message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE &&
           notification()->title().starts_with(u"Copied ") &&
           notification()->display_source() ==
               l10n_util::GetStringUTF16(
                   IDS_ASH_MESSAGE_CENTER_SYSTEM_APP_NAME_FILES) &&
           !notification()->buttons().empty() &&
           (notification()->buttons().front().title == u"Show in folder");
  }

  bool HaveMoveErrorNotification() {
    return notification().has_value() &&
           notification()->type() ==
               message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE &&
           notification()->title().starts_with(u"Can't move file") &&
           notification()->display_source() ==
               l10n_util::GetStringUTF16(
                   IDS_ASH_MESSAGE_CENTER_SYSTEM_APP_NAME_FILES);
  }

  bool HaveCopyErrorNotification() {
    return notification().has_value() &&
           notification()->type() ==
               message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE &&
           notification()->title().starts_with(u"Can't copy file") &&
           notification()->display_source() ==
               l10n_util::GetStringUTF16(
                   IDS_ASH_MESSAGE_CENTER_SYSTEM_APP_NAME_FILES);
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<StubNotificationDisplayService> display_service_;
  base::FilePath file_path_ = base::FilePath("/some/path/foo.doc");
};

TEST_F(CloudUploadNotificationManagerTest,
       DoesNothingWhenCreatedAndImmediatelyClosed) {
  scoped_refptr<CloudUploadNotificationManager> manager =
      base::MakeRefCounted<CloudUploadNotificationManager>(
          profile(), "Google Drive", "Google Docs", 1, UploadType::kMove);

  manager->CloseNotification();
}

TEST_F(CloudUploadNotificationManagerTest,
       ShowUploadProgressCreatesNotificationForMove) {
  scoped_refptr<CloudUploadNotificationManager> manager =
      base::MakeRefCounted<CloudUploadNotificationManager>(
          profile(), "Google Drive", "Google Docs", 1, UploadType::kMove);

  ASSERT_EQ(std::nullopt, notification());
  manager->ShowUploadProgress(1);
  ASSERT_TRUE(HaveMoveProgressNotification());

  manager->CloseNotification();
}

TEST_F(CloudUploadNotificationManagerTest,
       ShowUploadProgressCreatesNotificationForCopy) {
  scoped_refptr<CloudUploadNotificationManager> manager =
      base::MakeRefCounted<CloudUploadNotificationManager>(
          profile(), "Google Drive", "Google Docs", 1, UploadType::kCopy);

  ASSERT_EQ(std::nullopt, notification());
  manager->ShowUploadProgress(1);
  ASSERT_TRUE(HaveCopyProgressNotification());

  manager->CloseNotification();
}

TEST_F(CloudUploadNotificationManagerTest, MinimumTimingForMove) {
  scoped_refptr<CloudUploadNotificationManager> manager =
      base::MakeRefCounted<CloudUploadNotificationManager>(
          profile(), "Google Drive", "Google Docs", 1, UploadType::kMove);

  manager->ShowUploadProgress(1);
  manager->ShowUploadProgress(100);
  manager->SetDestinationPath(file_path_);
  manager->MarkUploadComplete();
  ASSERT_TRUE(HaveMoveProgressNotification());

  // The progress notification should still be showing.
  task_environment_.FastForwardBy(base::Milliseconds(4900));
  ASSERT_TRUE(HaveMoveProgressNotification());

  // Now we see the Complete nofication after 5s.
  task_environment_.FastForwardBy(base::Milliseconds(500));
  ASSERT_TRUE(HaveMoveCompleteNotification());

  // Now we're at 9900 ms total - we still expect the Complete notification.
  task_environment_.FastForwardBy(base::Milliseconds(4500));
  ASSERT_TRUE(HaveMoveCompleteNotification());

  // After > 10s total, the notification should be closed.
  task_environment_.FastForwardBy(base::Milliseconds(500));
  ASSERT_EQ(std::nullopt, notification());
}

TEST_F(CloudUploadNotificationManagerTest, MinimumTimingForCopy) {
  scoped_refptr<CloudUploadNotificationManager> manager =
      base::MakeRefCounted<CloudUploadNotificationManager>(
          profile(), "Google Drive", "Google Docs", 1, UploadType::kCopy);

  manager->ShowUploadProgress(1);
  manager->ShowUploadProgress(100);
  manager->SetDestinationPath(file_path_);
  manager->MarkUploadComplete();
  ASSERT_TRUE(HaveCopyProgressNotification());

  // The progress notification should still be showing.
  task_environment_.FastForwardBy(base::Milliseconds(4900));
  ASSERT_TRUE(HaveCopyProgressNotification());

  // Now we see the Complete nofication after 5s.
  task_environment_.FastForwardBy(base::Milliseconds(500));
  ASSERT_TRUE(HaveCopyCompleteNotification());

  // Now we're at 9900 ms total - we still expect the Complete notification.
  task_environment_.FastForwardBy(base::Milliseconds(4500));
  ASSERT_TRUE(HaveCopyCompleteNotification());

  // After > 10s total, the notification should be closed.
  task_environment_.FastForwardBy(base::Milliseconds(500));
  ASSERT_EQ(std::nullopt, notification());
}

TEST_F(CloudUploadNotificationManagerTest, CompleteWithoutProgress) {
  scoped_refptr<CloudUploadNotificationManager> manager =
      base::MakeRefCounted<CloudUploadNotificationManager>(
          profile(), "Google Drive", "Google Docs", 1, UploadType::kMove);

  manager->SetDestinationPath(file_path_);
  manager->MarkUploadComplete();
  ASSERT_TRUE(HaveMoveCompleteNotification());

  // The complete notification should still be showing.
  task_environment_.FastForwardBy(base::Milliseconds(4900));
  ASSERT_TRUE(HaveMoveCompleteNotification());

  // After > 5s total, the notification should be closed.
  task_environment_.FastForwardBy(base::Milliseconds(500));
  ASSERT_EQ(std::nullopt, notification());
}

TEST_F(CloudUploadNotificationManagerTest, CancelClick) {
  base::RunLoop run_loop;
  base::OnceClosure cancel_callback = run_loop.QuitClosure();

  scoped_refptr<CloudUploadNotificationManager> manager =
      base::MakeRefCounted<CloudUploadNotificationManager>(
          profile(), "Google Drive", "Google Docs", 1, UploadType::kMove);

  manager->SetCancelCallback(std::move(cancel_callback));
  manager->ShowUploadProgress(1);
  ASSERT_TRUE(HaveMoveProgressNotificationWithCancelButton());

  // Click "Cancel" button (0th button) which triggers |cancel_callback|.
  display_service_->SimulateClick(NotificationHandler::Type::TRANSIENT,
                                  notification()->id(), /*action_index=*/0,
                                  std::nullopt);

  // Run loop until |cancel_callback| is called.
  run_loop.Run();
  manager->CloseNotification();
}

TEST_F(CloudUploadNotificationManagerTest,
       CancelButtonDisappearsAfterProgressComplete) {
  scoped_refptr<CloudUploadNotificationManager> manager =
      base::MakeRefCounted<CloudUploadNotificationManager>(
          profile(), "Google Drive", "Google Docs", 1, UploadType::kMove);

  // TODO(b/244396230): remove CancelCallback once button always set for both
  // Clouds.
  manager->SetCancelCallback(base::DoNothing());
  manager->ShowUploadProgress(1);
  ASSERT_TRUE(HaveMoveProgressNotificationWithCancelButton());
  manager->MarkUploadComplete();
  ASSERT_TRUE(HaveMoveProgressNotificationWithoutCancelButton());
  manager->CloseNotification();
}

TEST_F(CloudUploadNotificationManagerTest,
       CancelButtonRemainsAfterMinimumTime) {
  scoped_refptr<CloudUploadNotificationManager> manager =
      base::MakeRefCounted<CloudUploadNotificationManager>(
          profile(), "Google Drive", "Google Docs", 1, UploadType::kMove);

  // TODO(b/244396230): remove CancelCallback once button always set for both
  // Clouds.
  manager->SetCancelCallback(base::DoNothing());
  manager->ShowUploadProgress(1);
  ASSERT_TRUE(HaveMoveProgressNotificationWithCancelButton());

  // The Cancel button should still appear after the minimum progress
  // notification time of 5s.
  task_environment_.FastForwardBy(base::Milliseconds(6000));
  ASSERT_TRUE(HaveMoveProgressNotificationWithCancelButton());

  manager->CloseNotification();
}

TEST_F(CloudUploadNotificationManagerTest, ShowInFolderClick) {
  scoped_refptr<CloudUploadNotificationManager> manager =
      base::MakeRefCounted<CloudUploadNotificationManager>(
          profile(), "Google Drive", "Google Docs", 1, UploadType::kMove);

  manager->SetDestinationPath(file_path_);
  manager->MarkUploadComplete();
  ASSERT_TRUE(HaveMoveCompleteNotification());

  base::RunLoop run_loop;
  manager->SetHandleCompleteNotificationClickCallbackForTesting(
      base::BindLambdaForTesting(
          [&run_loop, this](base::FilePath destination_path) {
            // Check |destination_path| is as expected.
            EXPECT_EQ(destination_path, file_path_);
            run_loop.Quit();
          }));

  // Click "Show in folder" button (0th button) which triggers
  // |HandleNotificationClick|.
  display_service_->SimulateClick(NotificationHandler::Type::TRANSIENT,
                                  notification()->id(), /*action_index=*/0,
                                  std::nullopt);

  // Run loop until |HandleNotificationClick| is called.
  run_loop.Run();
  manager->CloseNotification();
}

TEST_F(CloudUploadNotificationManagerTest, ErrorStaysOpenForMove) {
  scoped_refptr<CloudUploadNotificationManager> manager =
      base::MakeRefCounted<CloudUploadNotificationManager>(
          profile(), "Google Drive", "Google Docs", 1, UploadType::kMove);

  manager->ShowUploadProgress(1);
  manager->ShowUploadProgress(100);
  manager->ShowUploadError("error");
  // The error is shown straight away.
  ASSERT_TRUE(HaveMoveErrorNotification());

  // The error notification should still be showing.
  task_environment_.FastForwardBy(base::Seconds(60));
  ASSERT_TRUE(HaveMoveErrorNotification());

  manager->CloseNotification();
}

TEST_F(CloudUploadNotificationManagerTest, ErrorStaysOpenForCopy) {
  scoped_refptr<CloudUploadNotificationManager> manager =
      base::MakeRefCounted<CloudUploadNotificationManager>(
          profile(), "Google Drive", "Google Docs", 1, UploadType::kCopy);

  manager->ShowUploadProgress(1);
  manager->ShowUploadProgress(100);
  manager->ShowUploadError("error");
  // The error is shown straight away.
  ASSERT_TRUE(HaveCopyErrorNotification());

  // The error notification should still be showing.
  task_environment_.FastForwardBy(base::Seconds(60));
  ASSERT_TRUE(HaveCopyErrorNotification());

  manager->CloseNotification();
}

TEST_F(CloudUploadNotificationManagerTest, ManagerLifetime) {
  {
    scoped_refptr<CloudUploadNotificationManager> manager =
        base::MakeRefCounted<CloudUploadNotificationManager>(
            profile(), "Google Drive", "Google Docs", 1, UploadType::kMove);

    manager->ShowUploadProgress(1);
    manager->ShowUploadError("error");
    ASSERT_TRUE(HaveMoveErrorNotification());
  }
  // We still have a ref to manager until the notification is dismissed.
  ASSERT_TRUE(HaveMoveErrorNotification());

  notification()->delegate()->Click(std::nullopt, std::nullopt);
  ASSERT_EQ(std::nullopt, notification());
}

}  // namespace ash::cloud_upload
