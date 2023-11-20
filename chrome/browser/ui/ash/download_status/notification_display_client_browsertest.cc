// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/download_status/display_test_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/download_status_updater.mojom.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash::download_status {

namespace {

// Alias -----------------------------------------------------------------------

using ::testing::Contains;
using ::testing::Mock;
using ::testing::Not;
using ::testing::WithArg;

// MockNotificationDisplayServiceObserver --------------------------------------

// NOTE: When a download notification is closed, `OnNotificationClosed()` is not
// called because the notification's handler type is `TRANSIENT`.
class MockNotificationDisplayServiceObserver
    : public NotificationDisplayService::Observer {
 public:
  MOCK_METHOD(void,
              OnNotificationDisplayed,
              (const message_center::Notification&,
               const NotificationCommon::Metadata*),
              (override));
  MOCK_METHOD(void, OnNotificationClosed, (const std::string&), (override));
  MOCK_METHOD(void,
              OnNotificationDisplayServiceDestroyed,
              (NotificationDisplayService * service),
              (override));
};

// Helpers ---------------------------------------------------------------------

NotificationDisplayService* GetNotificationDisplayService() {
  return NotificationDisplayServiceFactory::GetInstance()->GetForProfile(
      ProfileManager::GetActiveUserProfile());
}

// Returns the IDs of the displayed notifications.
std::set<std::string> GetDisplayedNotificationIds() {
  base::test::TestFuture<std::set<std::string>> future;
  GetNotificationDisplayService()->GetDisplayed(
      base::BindLambdaForTesting([&future](std::set<std::string> ids, bool) {
        future.SetValue(std::move(ids));
      }));
  return future.Get();
}

}  // namespace

class NotificationDisplayClientBrowserTest : public InProcessBrowserTest {
 public:
  NotificationDisplayClientBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kSysUiDownloadsIntegrationV2);
  }

  // Updates download through the download status updater.
  void Update(crosapi::mojom::DownloadStatusPtr status) {
    download_status_updater_remote_->Update(std::move(status));
    download_status_updater_remote_.FlushForTesting();
  }

  MockNotificationDisplayServiceObserver& service_observer() {
    return service_observer_;
  }

 private:
  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    crosapi::CrosapiManager::Get()->crosapi_ash()->BindDownloadStatusUpdater(
        download_status_updater_remote_.BindNewPipeAndPassReceiver());
    service_observation_.Observe(GetNotificationDisplayService());
  }

  void TearDownOnMainThread() override {
    service_observation_.Reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  mojo::Remote<crosapi::mojom::DownloadStatusUpdater>
      download_status_updater_remote_;
  MockNotificationDisplayServiceObserver service_observer_;
  base::ScopedObservation<NotificationDisplayService,
                          NotificationDisplayService::Observer>
      service_observation_{&service_observer_};
};

// Verifies that when an in-progress download is cancelled, its notification
// should be removed.
IN_PROC_BROWSER_TEST_F(NotificationDisplayClientBrowserTest, CancelDownload) {
  std::string notification_id;
  EXPECT_CALL(service_observer(), OnNotificationDisplayed)
      .WillOnce(WithArg<0>(
          [&notification_id](const message_center::Notification& notification) {
            notification_id = notification.id();
          }));
  crosapi::mojom::DownloadStatusPtr download = CreateInProgressDownloadStatus(
      ProfileManager::GetActiveUserProfile(), /*received_bytes=*/0,
      /*target_bytes=*/1024);
  Update(download->Clone());
  Mock::VerifyAndClearExpectations(&service_observer());

  download->state = crosapi::mojom::DownloadState::kCancelled;
  Update(download->Clone());
  EXPECT_THAT(GetDisplayedNotificationIds(), Not(Contains(notification_id)));
}

// Verifies that when an in-progress download completes, its notification should
// still show.
IN_PROC_BROWSER_TEST_F(NotificationDisplayClientBrowserTest, CompleteDownload) {
  std::string notification_id;
  EXPECT_CALL(service_observer(), OnNotificationDisplayed)
      .WillOnce(WithArg<0>(
          [&notification_id](const message_center::Notification& notification) {
            notification_id = notification.id();
          }));
  crosapi::mojom::DownloadStatusPtr download = CreateInProgressDownloadStatus(
      ProfileManager::GetActiveUserProfile(), /*received_bytes=*/0,
      /*target_bytes=*/1024);
  Update(download->Clone());
  Mock::VerifyAndClearExpectations(&service_observer());

  download->state = crosapi::mojom::DownloadState::kComplete;
  Update(download->Clone());
  EXPECT_THAT(GetDisplayedNotificationIds(), Contains(notification_id));
}

// Verifies that when an in-progress download is interrupted, its notification
// should be removed.
IN_PROC_BROWSER_TEST_F(NotificationDisplayClientBrowserTest,
                       InterruptDownload) {
  std::string notification_id;
  EXPECT_CALL(service_observer(), OnNotificationDisplayed)
      .WillOnce(WithArg<0>(
          [&notification_id](const message_center::Notification& notification) {
            notification_id = notification.id();
          }));
  crosapi::mojom::DownloadStatusPtr download = CreateInProgressDownloadStatus(
      ProfileManager::GetActiveUserProfile(), /*received_bytes=*/0,
      /*target_bytes=*/1024);
  Update(download->Clone());
  Mock::VerifyAndClearExpectations(&service_observer());

  download->state = crosapi::mojom::DownloadState::kInterrupted;
  Update(download->Clone());
  EXPECT_THAT(GetDisplayedNotificationIds(), Not(Contains(notification_id)));
}

}  // namespace ash::download_status
