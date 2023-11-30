// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/message_center/ash_message_popup_collection.h"
#include "ash/system/message_center/ash_notification_view.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/status_area_widget.h"
#include "ash/test/view_drawn_waiter.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/profile_notification.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/ash_test_util.h"
#include "chrome/browser/ui/ash/download_status/display_test_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/download_status_updater.mojom.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/views/message_popup_view.h"
#include "ui/message_center/views/notification_control_buttons_view.h"

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

// Verifies that a download notification should not show again if it has been
// closed by user.
IN_PROC_BROWSER_TEST_F(NotificationDisplayClientBrowserTest,
                       DoNotShowAfterCloseByUser) {
  std::string notification_id;
  EXPECT_CALL(service_observer(), OnNotificationDisplayed)
      .WillOnce(WithArg<0>(
          [&notification_id](const message_center::Notification& notification) {
            notification_id = notification.id();
          }));
  Profile* const profile = ProfileManager::GetActiveUserProfile();
  crosapi::mojom::DownloadStatusPtr download =
      CreateInProgressDownloadStatus(profile, /*received_bytes=*/0,
                                     /*target_bytes=*/1024);
  Update(download->Clone());
  Mock::VerifyAndClearExpectations(&service_observer());

  // Wait until `popup_collection` becomes idle.
  AshMessagePopupCollection* const popup_collection =
      Shell::GetPrimaryRootWindowController()
          ->shelf()
          ->GetStatusAreaWidget()
          ->notification_center_tray()
          ->popup_collection();
  base::test::TestFuture<void> future;
  popup_collection->SetAnimationIdleClosureForTest(future.GetCallback());
  future.Get();

  // NOTE: The notification ID associated with the view differs from
  // `notification_id` as it incorporates the profile ID.
  message_center::MessagePopupView* const popup_view =
      popup_collection->GetPopupViewForNotificationID(
          ProfileNotification::GetProfileNotificationId(
              notification_id, ProfileNotification::GetProfileID(profile)));
  ASSERT_TRUE(popup_view);
  message_center::MessageView* const message_view = popup_view->message_view();
  ASSERT_TRUE(message_view);

  // Move mouse to `message_view` until `close_button` shows and then click
  // `close_button` to remove the notification associated with
  // `notification_id`.
  test::MoveMouseTo(message_view);
  views::View* const close_button =
      views::AsViewClass<AshNotificationView>(message_view)
          ->control_buttons_view_for_test()
          ->close_button();
  ViewDrawnWaiter().Wait(close_button);
  test::Click(close_button, ui::EF_NONE);

  // The notification associated with `notification_id` should not display.
  EXPECT_THAT(GetDisplayedNotificationIds(), Not(Contains(notification_id)));

  // Update the same notification after closing. The closed notification should
  // not show again.
  Update(download->Clone());
  EXPECT_THAT(GetDisplayedNotificationIds(), Not(Contains(notification_id)));
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
