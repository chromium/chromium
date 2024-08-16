// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/notification_center/ash_message_popup_collection.h"
#include "ash/system/notification_center/message_popup_animation_waiter.h"
#include "ash/system/notification_center/notification_center_test_api.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/notification_center/views/ash_notification_view.h"
#include "ash/system/status_area_widget.h"
#include "ash/test/view_drawn_waiter.h"
#include "ash/webui/media_app_ui/url_constants.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/mock_download_status_updater_client.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_browsertest_base.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/profile_notification.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/download_status/display_metadata.h"
#include "chrome/browser/ui/ash/download_status/display_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ash/util/ash_test_util.h"
#include "chromeos/crosapi/mojom/download_status_updater.mojom.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/aura/env.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/test/find_window.h"
#include "ui/aura/window.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/views/large_image_view.h"
#include "ui/message_center/views/message_popup_view.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/message_center/views/notification_view_base.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/view_utils.h"

namespace ash::download_status {

namespace {

// Alias -----------------------------------------------------------------------

using ::testing::_;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::WithArg;

// MockEnvObserver -------------------------------------------------------------

class MockEnvObserver : public aura::EnvObserver {
 public:
  MOCK_METHOD(void, OnWindowInitialized, (aura::Window*), (override));
};

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

// MockTabStripModelObserver ---------------------------------------------------

class MockTabStripModelObserver : public TabStripModelObserver {
 public:
  MOCK_METHOD(void, OnTabWillBeAdded, (), (override));
};

// Helpers ---------------------------------------------------------------------

// Returns the text ID for the given `command_type`.
int GetCommandTextId(CommandType command_type) {
  switch (command_type) {
    case CommandType::kCancel:
      return IDS_ASH_DOWNLOAD_COMMAND_TEXT_CANCEL;
    case CommandType::kCopyToClipboard:
      return IDS_ASH_DOWNLOAD_COMMAND_TEXT_COPY_TO_CLIPBOARD;
    case CommandType::kEditWithMediaApp:
      return IDS_DOWNLOAD_NOTIFICATION_LABEL_OPEN_AND_EDIT;
    case CommandType::kOpenFile:
      NOTREACHED();
    case CommandType::kOpenWithMediaApp:
      return IDS_DOWNLOAD_NOTIFICATION_LABEL_OPEN;
    case CommandType::kPause:
      return IDS_ASH_DOWNLOAD_COMMAND_TEXT_PAUSE;
    case CommandType::kResume:
      return IDS_ASH_DOWNLOAD_COMMAND_TEXT_RESUME;
    case CommandType::kShowInBrowser:
      NOTREACHED();
    case CommandType::kShowInFolder:
      return IDS_ASH_DOWNLOAD_COMMAND_TEXT_SHOW_IN_FOLDER;
    case CommandType::kViewDetailsInBrowser:
      return IDS_ASH_DOWNLOAD_COMMAND_TEXT_VIEW_DETAILS_IN_BROWSER;
  }
}

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

// Returns the notification popup view specified by 'notification_id'. Returns
// `nullptr` if such a view cannot be found.
AshNotificationView* GetPopupView(Profile* profile,
                                  const std::string& notification_id) {
  // Wait until `popup_collection` becomes idle.
  AshMessagePopupCollection* const popup_collection =
      Shell::GetPrimaryRootWindowController()
          ->shelf()
          ->GetStatusAreaWidget()
          ->notification_center_tray()
          ->popup_collection();
  if (!popup_collection) {
    return nullptr;
  }
  MessagePopupAnimationWaiter(popup_collection).Wait();

  // NOTE: The notification ID associated with the view differs from
  // `notification_id` as it incorporates the profile ID.
  auto* const popup_view = NotificationCenterTestApi().GetPopupViewForId(
      ProfileNotification::GetProfileNotificationId(
          notification_id, ProfileNotification::GetProfileID(profile)));
  return popup_view ? views::AsViewClass<AshNotificationView>(
                          popup_view->message_view())
                    : nullptr;
}

}  // namespace

class NotificationDisplayClientBrowserTest
    : public SystemWebAppBrowserTestBase {
 public:
  // Updates download through the download status updater.
  void Update(crosapi::mojom::DownloadStatusPtr status) {
    download_status_updater_remote_->Update(std::move(status));
    download_status_updater_remote_.FlushForTesting();
  }

  crosapi::MockDownloadStatusUpdaterClient& download_status_updater_client() {
    return download_status_updater_client_;
  }

  MockNotificationDisplayServiceObserver& service_observer() {
    return service_observer_;
  }

 private:
  // SystemWebAppBrowserTestBase:
  void SetUpOnMainThread() override {
    SystemWebAppBrowserTestBase::SetUpOnMainThread();

    crosapi::CrosapiManager::Get()->crosapi_ash()->BindDownloadStatusUpdater(
        download_status_updater_remote_.BindNewPipeAndPassReceiver());
    download_status_updater_remote_->BindClient(
        download_status_updater_client_receiver_
            .BindNewPipeAndPassRemoteWithVersion());
    download_status_updater_remote_.FlushForTesting();

    service_observation_.Observe(GetNotificationDisplayService());
  }

  void TearDownOnMainThread() override {
    service_observation_.Reset();
    SystemWebAppBrowserTestBase::TearDownOnMainThread();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  mojo::Remote<crosapi::mojom::DownloadStatusUpdater>
      download_status_updater_remote_;
  MockNotificationDisplayServiceObserver service_observer_;
  base::ScopedObservation<NotificationDisplayService,
                          NotificationDisplayService::Observer>
      service_observation_{&service_observer_};

  // The client bound to the download status updater under test.
  crosapi::MockDownloadStatusUpdaterClient download_status_updater_client_;
  mojo::Receiver<crosapi::mojom::DownloadStatusUpdaterClient>
      download_status_updater_client_receiver_{
          &download_status_updater_client_};
};

// Verifies that when an in-progress download is cancelled, its notification
// should be removed.
IN_PROC_BROWSER_TEST_F(NotificationDisplayClientBrowserTest, CancelDownload) {
  // Add a download that is not cancellable. Cache the notification ID.
  Profile* const profile = ProfileManager::GetActiveUserProfile();
  std::string notification_id;
  EXPECT_CALL(service_observer(), OnNotificationDisplayed)
      .WillOnce(WithArg<0>(
          [&notification_id](const message_center::Notification& notification) {
            notification_id = notification.id();
          }));
  crosapi::mojom::DownloadStatusPtr uncancellable_download =
      CreateInProgressDownloadStatus(profile,
                                     /*extension=*/"txt",
                                     /*received_bytes=*/0,
                                     /*total_bytes=*/1024);
  uncancellable_download->cancellable = false;
  Update(uncancellable_download->Clone());
  Mock::VerifyAndClearExpectations(&service_observer());

  // Check that the notification view of `uncancellable_download` does not have
  // a cancel button.
  AshNotificationView* const uncancellable_download_notification =
      GetPopupView(profile, notification_id);
  ASSERT_TRUE(uncancellable_download_notification);
  std::vector<raw_ptr<views::LabelButton, VectorExperimental>> action_buttons =
      uncancellable_download_notification->GetActionButtonsForTest();
  const std::u16string cancel_button_text =
      l10n_util::GetStringUTF16(GetCommandTextId(CommandType::kCancel));
  auto cancel_button_iter = base::ranges::find(
      action_buttons, cancel_button_text, &views::LabelButton::GetText);
  EXPECT_EQ(cancel_button_iter, action_buttons.end());

  // Add a cancellable download. Cache the notification ID.
  EXPECT_CALL(service_observer(), OnNotificationDisplayed)
      .WillOnce(WithArg<0>(
          [&notification_id](const message_center::Notification& notification) {
            notification_id = notification.id();
          }));
  crosapi::mojom::DownloadStatusPtr cancellable_download =
      CreateInProgressDownloadStatus(profile,
                                     /*extension=*/"txt",
                                     /*received_bytes=*/0,
                                     /*total_bytes=*/1024);
  cancellable_download->cancellable = true;
  Update(cancellable_download->Clone());
  Mock::VerifyAndClearExpectations(&service_observer());

  // Implement download cancellation for the mock client.
  base::RunLoop run_loop;
  ON_CALL(download_status_updater_client(),
          Cancel(cancellable_download->guid, _))
      .WillByDefault(
          [&](const std::string& guid,
              crosapi::MockDownloadStatusUpdaterClient::CancelCallback
                  callback) {
            cancellable_download->cancellable = false;
            cancellable_download->state =
                crosapi::mojom::DownloadState::kCancelled;
            Update(cancellable_download->Clone());
            std::move(callback).Run(/*handled=*/true);
            run_loop.Quit();
          });

  // Get the cancel button.
  AshNotificationView* const cancellable_download_notification =
      GetPopupView(profile, notification_id);
  ASSERT_TRUE(cancellable_download_notification);
  action_buttons = cancellable_download_notification->GetActionButtonsForTest();
  cancel_button_iter = base::ranges::find(action_buttons, cancel_button_text,
                                          &views::LabelButton::GetText);
  ASSERT_NE(cancel_button_iter, action_buttons.end());

  // Click on the cancel button and wait until download is cancelled.
  base::UserActionTester tester;
  test::Click(*cancel_button_iter, ui::EF_NONE);
  run_loop.Run();

  // After download cancellation, the associated notification should be removed.
  // The button click should be recorded.
  EXPECT_THAT(GetDisplayedNotificationIds(), Not(Contains(notification_id)));
  EXPECT_EQ(tester.GetActionCount("DownloadNotificationV2.Button_Cancel"), 1);
}

// Verifies clicking a completed download's notification.
IN_PROC_BROWSER_TEST_F(NotificationDisplayClientBrowserTest,
                       ClickCompletedDownload) {
  // Wait until test system apps are installed so that opening a download file
  // is handled.
  WaitForTestSystemAppInstall();

  // Add a completed download and cache its notification ID.
  Profile* const profile = ProfileManager::GetActiveUserProfile();
  std::string notification_id;
  EXPECT_CALL(service_observer(), OnNotificationDisplayed)
      .WillOnce(WithArg<0>(
          [&notification_id](const message_center::Notification& notification) {
            notification_id = notification.id();
          }));
  crosapi::mojom::DownloadStatusPtr download = CreateDownloadStatus(
      profile, /*extension=*/"txt", crosapi::mojom::DownloadState::kComplete,
      crosapi::mojom::DownloadProgress::New(
          /*loop=*/false,
          /*received_bytes=*/1024,
          /*total_bytes=*/1024,
          /*visible=*/false));
  Update(download->Clone());
  Mock::VerifyAndClearExpectations(&service_observer());

  // The command that shows downloads in browser should not be performed.
  EXPECT_CALL(download_status_updater_client(), ShowInBrowser).Times(0);

  // Set up an observer to wait until a tab is added.
  NiceMock<MockTabStripModelObserver> tab_strip_model_observer;
  base::ScopedObservation<TabStripModel, MockTabStripModelObserver>
      tab_strip_model_observation{&tab_strip_model_observer};
  base::test::TestFuture<void> future;
  ON_CALL(tab_strip_model_observer, OnTabWillBeAdded)
      .WillByDefault(base::test::RunClosure(future.GetRepeatingCallback()));

  AshNotificationView* const notification_view =
      GetPopupView(profile, notification_id);
  ASSERT_TRUE(notification_view);

  // Click `notification_view` and wait until a tab is added. Then verify that
  // the click is recorded. It assumes the download file is opened by browser.
  base::UserActionTester tester;
  tab_strip_model_observation.Observe(browser()->tab_strip_model());
  test::Click(notification_view, ui::EF_NONE);
  EXPECT_TRUE(future.Wait());
  Mock::VerifyAndClearExpectations(&tab_strip_model_observer);
  Mock::VerifyAndClearExpectations(&download_status_updater_client());
  EXPECT_EQ(tester.GetActionCount("DownloadNotificationV2.Click_Completed"), 1);
}

// Verifies clicking an in-progress download's notification.
IN_PROC_BROWSER_TEST_F(NotificationDisplayClientBrowserTest,
                       ClickInProgressDownload) {
  // Add an in-progress download and cache its notification ID.
  Profile* const profile = ProfileManager::GetActiveUserProfile();
  std::string notification_id;
  EXPECT_CALL(service_observer(), OnNotificationDisplayed)
      .WillOnce(WithArg<0>(
          [&notification_id](const message_center::Notification& notification) {
            notification_id = notification.id();
          }));
  crosapi::mojom::DownloadStatusPtr download =
      CreateInProgressDownloadStatus(profile,
                                     /*extension=*/"txt",
                                     /*received_bytes=*/0,
                                     /*total_bytes=*/1024);
  Update(download->Clone());
  Mock::VerifyAndClearExpectations(&service_observer());

  AshNotificationView* const notification_view =
      GetPopupView(profile, notification_id);
  ASSERT_TRUE(notification_view);

  // Click `notification_view` and wait until showing downloads in browser. Then
  // verify that the click is recorded.
  base::UserActionTester tester;
  base::RunLoop run_loop;
  EXPECT_CALL(download_status_updater_client(),
              ShowInBrowser(download->guid, _))
      .WillOnce(
          [&run_loop](
              const std::string& guid,
              crosapi::MockDownloadStatusUpdaterClient::ShowInBrowserCallback
                  callback) {
            std::move(callback).Run(/*handled=*/true);
            run_loop.Quit();
          });
  test::Click(notification_view, ui::EF_NONE);
  run_loop.Run();
  Mock::VerifyAndClearExpectations(&download_status_updater_client());
  EXPECT_EQ(tester.GetActionCount("DownloadNotificationV2.Click_InProgress"),
            1);
}

// Verifies that when an in-progress download completes, its notification should
// still show.
IN_PROC_BROWSER_TEST_F(NotificationDisplayClientBrowserTest, CompleteDownload) {
  Profile* const profile = ProfileManager::GetActiveUserProfile();
  crosapi::mojom::DownloadStatusPtr download = CreateDownloadStatus(
      profile,
      /*extension=*/"txt", crosapi::mojom::DownloadState::kInProgress,
      /*progress=*/nullptr);
  EXPECT_FALSE(download->target_file_path);
  std::string notification_id;

  // When the notification for `download` displays:
  // 1. Check the notification's properties. Since the download target file path
  //    is unavailable, the primary text should be the display name of the file
  //    referenced by the full path.
  // 2. Cache the notification ID.
  EXPECT_CALL(
      service_observer(),
      OnNotificationDisplayed(
          AllOf(
              Property(&message_center::Notification::progress_status,
                       Eq(std::u16string())),
              Property(&message_center::Notification::title,
                       Eq(download->full_path->BaseName().LossyDisplayName()))),
          _))
      .WillOnce(WithArg<0>(
          [&notification_id](const message_center::Notification& notification) {
            notification_id = notification.id();
          }));
  Update(download->Clone());
  Mock::VerifyAndClearExpectations(&service_observer());

  // The progress of `download` is not set so the progress bar should not show.
  AshNotificationView* popup_view = GetPopupView(profile, notification_id);
  ASSERT_TRUE(popup_view);
  EXPECT_FALSE(popup_view->progress_bar_view_for_testing());

  // Update the download's received bytes and total bytes. Then check the
  // notification's progress.
  download->progress = crosapi::mojom::DownloadProgress::New();
  crosapi::mojom::DownloadProgressPtr& progress = download->progress;
  progress->received_bytes = 0;
  progress->total_bytes = 1024;
  progress->visible = true;
  EXPECT_CALL(
      service_observer(),
      OnNotificationDisplayed(
          AllOf(
              Property(&message_center::Notification::id, Eq(notification_id)),
              Property(&message_center::Notification::progress, Eq(0))),
          _));
  Update(download->Clone());
  Mock::VerifyAndClearExpectations(&service_observer());

  // The notification view should have a visible progress bar because `progress`
  // specifies the visibility to be true.
  popup_view = GetPopupView(profile, notification_id);
  ASSERT_TRUE(popup_view);
  const views::ProgressBar* progress_bar =
      popup_view->progress_bar_view_for_testing();
  ASSERT_TRUE(progress_bar);
  EXPECT_TRUE(progress_bar->GetVisible());

  // Update the download's:
  // 1. Received bytes
  // 2. Status text
  // 3. Target file path
  // Then check the notification's properties.
  progress->received_bytes = 512;
  download->status_text = u"Random text";
  download->target_file_path = test::CreateFile(profile);
  EXPECT_NE(download->target_file_path, download->full_path);
  EXPECT_CALL(
      service_observer(),
      OnNotificationDisplayed(
          AllOf(
              Property(&message_center::Notification::id, Eq(notification_id)),
              Property(&message_center::Notification::progress, Eq(50)),
              Property(&message_center::Notification::title,
                       Eq(download->target_file_path->BaseName()
                              .LossyDisplayName()))),
          _));
  Update(download->Clone());
  Mock::VerifyAndClearExpectations(&service_observer());

  // Verify that the notification view of an in-progress download has a visible
  // progress bar with the expected status text.
  popup_view = GetPopupView(profile, notification_id);
  ASSERT_TRUE(popup_view);
  progress_bar = popup_view->progress_bar_view_for_testing();
  ASSERT_TRUE(progress_bar);
  EXPECT_TRUE(progress_bar->GetVisible());
  const views::Label* const status_view = popup_view->status_view_for_testing();
  ASSERT_TRUE(status_view);
  EXPECT_EQ(status_view->GetText(), u"Random text");

  // Complete download. Then check the notification.
  MarkDownloadStatusCompleted(*download);
  EXPECT_CALL(
      service_observer(),
      OnNotificationDisplayed(
          Property(&message_center::Notification::id, Eq(notification_id)), _));
  Update(download->Clone());
  EXPECT_THAT(GetDisplayedNotificationIds(), Contains(notification_id));

  // Verify that the notification view of a completed download does not have
  // a progress bar.
  popup_view = GetPopupView(profile, notification_id);
  ASSERT_TRUE(popup_view);
  EXPECT_FALSE(popup_view->progress_bar_view_for_testing());

  // Check the notification view's message label.
  const views::Label* const message_label =
      popup_view->message_label_for_testing();
  ASSERT_TRUE(message_label);
  EXPECT_EQ(message_label->GetText(), u"Random text");
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
      CreateInProgressDownloadStatus(profile,
                                     /*extension=*/"txt",
                                     /*received_bytes=*/0,
                                     /*total_bytes=*/1024);
  Update(download->Clone());
  Mock::VerifyAndClearExpectations(&service_observer());

  AshNotificationView* const notification_view =
      GetPopupView(profile, notification_id);
  ASSERT_TRUE(notification_view);

  // Move mouse to `notification_view` until `close_button` shows and then click
  // `close_button` to remove the notification associated with
  // `notification_id`.
  test::MoveMouseTo(notification_view);
  views::View* const close_button =
      notification_view->control_buttons_view_for_test()->close_button();
  ViewDrawnWaiter().Wait(close_button);
  test::Click(close_button, ui::EF_NONE);

  // The notification associated with `notification_id` should not display.
  EXPECT_THAT(GetDisplayedNotificationIds(), Not(Contains(notification_id)));

  // Update the same notification after closing. The closed notification should
  // not show again.
  Update(download->Clone());
  EXPECT_THAT(GetDisplayedNotificationIds(), Not(Contains(notification_id)));
}

// Verifies that the image download notification works as expected.
IN_PROC_BROWSER_TEST_F(NotificationDisplayClientBrowserTest, ImageDownload) {
  std::string notification_id;
  EXPECT_CALL(service_observer(), OnNotificationDisplayed)
      .WillOnce(WithArg<0>(
          [&notification_id](const message_center::Notification& notification) {
            notification_id = notification.id();
          }));

  // Create an image download.
  Profile* const profile = ProfileManager::GetActiveUserProfile();
  crosapi::mojom::DownloadStatusPtr download = CreateDownloadStatus(
      profile,
      /*extension=*/"png", crosapi::mojom::DownloadState::kInProgress,
      crosapi::mojom::DownloadProgress::New(
          /*loop=*/false, /*received_bytes=*/0,
          /*total_bytes=*/1024, /*visible=*/true));
  Update(download->Clone());
  Mock::VerifyAndClearExpectations(&service_observer());

  // Because `download` does not have an image, the notification image is null.
  AshNotificationView* popup_view = GetPopupView(profile, notification_id);
  ASSERT_TRUE(popup_view);
  EXPECT_FALSE(popup_view->GetViewByID(
      message_center::NotificationViewBase::kLargeImageView));

  // Update `download` with `image`.
  constexpr SkColor image_color = SK_ColorRED;
  const gfx::ImageSkia image =
      gfx::test::CreateImageSkia(/*size=*/100, image_color);
  download->image = image;
  Update(download->Clone());

  popup_view = GetPopupView(profile, notification_id);
  ASSERT_TRUE(popup_view);
  auto* const large_image_view =
      static_cast<message_center::LargeImageView*>(popup_view->GetViewByID(
          message_center::NotificationViewBase::kLargeImageView));
  ASSERT_TRUE(large_image_view);

  // Verify that the notification image is as expected.
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(
      *large_image_view->original_image().bitmap(),
      gfx::test::CreateBitmap(/*width=*/360,
                              /*height=*/240, image_color)));

  // An in-progress image download's notification should not have a 'Copy to
  // clipboard' button.
  const std::u16string copy_to_clipboard_button_text =
      l10n_util::GetStringUTF16(
          GetCommandTextId(CommandType::kCopyToClipboard));
  EXPECT_THAT(
      popup_view->GetActionButtonsForTest(),
      Not(Contains(Pointee(Property(&views::LabelButton::GetText,
                                    Eq(copy_to_clipboard_button_text))))));

  // Complete `download`. Then check action buttons.
  MarkDownloadStatusCompleted(*download);
  Update(download->Clone());
  const std::vector<raw_ptr<views::LabelButton, VectorExperimental>>
      action_buttons = popup_view->GetActionButtonsForTest();
  EXPECT_THAT(
      action_buttons,
      ElementsAre(
          Pointee(Property(&views::LabelButton::GetText,
                           Eq(l10n_util::GetStringUTF16(
                               GetCommandTextId(CommandType::kShowInFolder))))),
          Pointee(Property(&views::LabelButton::GetText,
                           Eq(copy_to_clipboard_button_text)))));

  // Click the 'Copy to clipboard' button. Then verify the click is recorded.
  base::UserActionTester tester;
  auto copy_to_clipboard_button_iter =
      base::ranges::find(action_buttons, copy_to_clipboard_button_text,
                         &views::LabelButton::GetText);
  ASSERT_NE(copy_to_clipboard_button_iter, action_buttons.cend());
  test::Click(*copy_to_clipboard_button_iter, ui::EF_NONE);
  EXPECT_EQ(
      tester.GetActionCount("DownloadNotificationV2.Button_CopyToClipboard"),
      1);

  // Verify the filename in the clipboard as expected.
  base::test::TestFuture<std::vector<ui::FileInfo>> test_future;
  ui::Clipboard::GetForCurrentThread()->ReadFilenames(
      ui::ClipboardBuffer::kCopyPaste,
      /*data_dst=*/nullptr, test_future.GetCallback());
  EXPECT_THAT(test_future.Get(),
              ElementsAre(Field(&ui::FileInfo::path, *download->full_path)));
}

// Verifies that the PDF download notification works as expected.
IN_PROC_BROWSER_TEST_F(NotificationDisplayClientBrowserTest, PdfDownload) {
  WaitForTestSystemAppInstall();

  std::string notification_id;
  EXPECT_CALL(service_observer(), OnNotificationDisplayed)
      .WillOnce(WithArg<0>(
          [&notification_id](const message_center::Notification& notification) {
            notification_id = notification.id();
          }));

  // Create an pdf download.
  Profile* const profile = ProfileManager::GetActiveUserProfile();
  crosapi::mojom::DownloadStatusPtr download = CreateDownloadStatus(
      profile,
      /*extension=*/"pdf", crosapi::mojom::DownloadState::kInProgress,
      crosapi::mojom::DownloadProgress::New(
          /*loop=*/false, /*received_bytes=*/0,
          /*total_bytes=*/1024, /*visible=*/true));
  Update(download->Clone());
  Mock::VerifyAndClearExpectations(&service_observer());

  AshNotificationView* popup_view = GetPopupView(profile, notification_id);
  ASSERT_TRUE(popup_view);

  // An in-progress PDF download's notification should not have a 'Open and
  // Edit' button.
  const std::u16string edit_text = l10n_util::GetStringUTF16(
      GetCommandTextId(CommandType::kEditWithMediaApp));
  EXPECT_THAT(popup_view->GetActionButtonsForTest(),
              Not(Contains(Pointee(
                  Property(&views::LabelButton::GetText, Eq(edit_text))))));

  // Complete `download`. Then check action buttons.
  MarkDownloadStatusCompleted(*download);
  Update(download->Clone());
  const std::vector<raw_ptr<views::LabelButton, VectorExperimental>>
      action_buttons = popup_view->GetActionButtonsForTest();
  EXPECT_THAT(
      action_buttons,
      ElementsAre(
          Pointee(Property(&views::LabelButton::GetText, Eq(edit_text))),
          Pointee(Property(&views::LabelButton::GetText,
                           Eq(l10n_util::GetStringUTF16(GetCommandTextId(
                               CommandType::kShowInFolder)))))));

  // Click the 'Open and edit' button. Then verify the click is recorded and
  // the Media App is launched.
  content::TestNavigationObserver observer =
      content::TestNavigationObserver(GURL(kChromeUIMediaAppURL));
  observer.StartWatchingNewWebContents();
  base::UserActionTester tester;
  auto edit_button_iter = base::ranges::find(action_buttons, edit_text,
                                             &views::LabelButton::GetText);
  ASSERT_NE(edit_button_iter, action_buttons.cend());
  test::Click(*edit_button_iter, ui::EF_NONE);
  EXPECT_EQ(
      tester.GetActionCount("DownloadNotificationV2.Button_EditWithMediaApp"),
      1);
  observer.Wait();
}

// Verifies that the audio download notification works as expected.
IN_PROC_BROWSER_TEST_F(NotificationDisplayClientBrowserTest, AudioDownload) {
  WaitForTestSystemAppInstall();

  std::string notification_id;
  EXPECT_CALL(service_observer(), OnNotificationDisplayed)
      .WillOnce(WithArg<0>(
          [&notification_id](const message_center::Notification& notification) {
            notification_id = notification.id();
          }));

  // Create an mp3 download.
  Profile* const profile = ProfileManager::GetActiveUserProfile();
  crosapi::mojom::DownloadStatusPtr download = CreateDownloadStatus(
      profile,
      /*extension=*/"mp3", crosapi::mojom::DownloadState::kInProgress,
      crosapi::mojom::DownloadProgress::New(
          /*loop=*/false, /*received_bytes=*/0,
          /*total_bytes=*/1024, /*visible=*/true));
  Update(download->Clone());
  Mock::VerifyAndClearExpectations(&service_observer());

  AshNotificationView* popup_view = GetPopupView(profile, notification_id);
  ASSERT_TRUE(popup_view);

  // An in-progress audio file download's notification should not have a 'Open'
  // button.
  const std::u16string open_text = l10n_util::GetStringUTF16(
      GetCommandTextId(CommandType::kOpenWithMediaApp));
  EXPECT_THAT(popup_view->GetActionButtonsForTest(),
              Not(Contains(Pointee(
                  Property(&views::LabelButton::GetText, Eq(open_text))))));

  // Complete `download`. Then check action buttons.
  MarkDownloadStatusCompleted(*download);
  Update(download->Clone());
  const std::vector<raw_ptr<views::LabelButton, VectorExperimental>>
      action_buttons = popup_view->GetActionButtonsForTest();
  EXPECT_THAT(
      action_buttons,
      ElementsAre(
          Pointee(Property(&views::LabelButton::GetText, Eq(open_text))),
          Pointee(Property(&views::LabelButton::GetText,
                           Eq(l10n_util::GetStringUTF16(GetCommandTextId(
                               CommandType::kShowInFolder)))))));

  // Click the 'Open' button. Then verify the click is recorded and the Media
  // App is launched.
  content::TestNavigationObserver observer =
      content::TestNavigationObserver(GURL(kChromeUIMediaAppURL));
  observer.StartWatchingNewWebContents();
  base::UserActionTester tester;
  auto open_button_iter = base::ranges::find(action_buttons, open_text,
                                             &views::LabelButton::GetText);
  ASSERT_NE(open_button_iter, action_buttons.cend());
  test::Click(*open_button_iter, ui::EF_NONE);
  EXPECT_EQ(
      tester.GetActionCount("DownloadNotificationV2.Button_OpenWithMediaApp"),
      1);
  observer.Wait();
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
  crosapi::mojom::DownloadStatusPtr download =
      CreateInProgressDownloadStatus(ProfileManager::GetActiveUserProfile(),
                                     /*extension=*/"txt",
                                     /*received_bytes=*/0,
                                     /*total_bytes=*/1024);
  Update(download->Clone());
  Mock::VerifyAndClearExpectations(&service_observer());

  download->state = crosapi::mojom::DownloadState::kInterrupted;
  Update(download->Clone());
  EXPECT_THAT(GetDisplayedNotificationIds(), Not(Contains(notification_id)));
}

// Verifies pausing and resuming download from a notification.
IN_PROC_BROWSER_TEST_F(NotificationDisplayClientBrowserTest,
                       PauseAndResumeDownload) {
  // Add a pausable download. Cache the notification ID.
  Profile* const profile = ProfileManager::GetActiveUserProfile();
  std::string notification_id;
  EXPECT_CALL(service_observer(), OnNotificationDisplayed)
      .WillOnce(WithArg<0>(
          [&notification_id](const message_center::Notification& notification) {
            notification_id = notification.id();
          }));
  crosapi::mojom::DownloadStatusPtr download =
      CreateInProgressDownloadStatus(profile,
                                     /*extension=*/"txt",
                                     /*received_bytes=*/0,
                                     /*total_bytes=*/1024);
  download->pausable = true;
  download->resumable = false;
  Update(download->Clone());
  Mock::VerifyAndClearExpectations(&service_observer());

  // The notification of a pausable download should not have a resume button.
  AshNotificationView* const popup_view =
      GetPopupView(profile, notification_id);
  ASSERT_TRUE(popup_view);
  std::vector<raw_ptr<views::LabelButton, VectorExperimental>> action_buttons =
      popup_view->GetActionButtonsForTest();
  const std::u16string resume_button_text =
      l10n_util::GetStringUTF16(GetCommandTextId(CommandType::kResume));
  auto resume_button_iter = base::ranges::find(
      action_buttons, resume_button_text, &views::LabelButton::GetText);
  EXPECT_EQ(resume_button_iter, action_buttons.end());

  // Implement download pause for the mock client.
  ON_CALL(download_status_updater_client(), Pause(download->guid, _))
      .WillByDefault([&](const std::string& guid,
                         crosapi::MockDownloadStatusUpdaterClient::PauseCallback
                             callback) {
        download->pausable = false;
        download->resumable = true;
        Update(download->Clone());
        std::move(callback).Run(/*handled=*/true);
      });

  // Get the pause button.
  const std::u16string pause_button_text =
      l10n_util::GetStringUTF16(GetCommandTextId(CommandType::kPause));
  auto pause_button_iter = base::ranges::find(action_buttons, pause_button_text,
                                              &views::LabelButton::GetText);
  ASSERT_NE(pause_button_iter, action_buttons.end());

  // Click on the pause button and wait until download is paused. Then verify
  // that the click is recorded.
  base::UserActionTester tester;
  auto run_loop = std::make_unique<base::RunLoop>();
  EXPECT_CALL(service_observer(), OnNotificationDisplayed)
      .WillOnce(WithArg<0>(
          [&run_loop](const message_center::Notification& notification) {
            run_loop->Quit();
          }));
  test::Click(*pause_button_iter, ui::EF_NONE);
  run_loop->Run();
  Mock::VerifyAndClearExpectations(&service_observer());
  EXPECT_EQ(tester.GetActionCount("DownloadNotificationV2.Button_Pause"), 1);

  // After pausing, `popup_view` is hidden. Therefore, get the notification view
  // from the notification center bubble.
  NotificationCenterTestApi().ToggleBubble();
  auto* const notification_view = views::AsViewClass<AshNotificationView>(
      NotificationCenterTestApi().GetNotificationViewForId(
          ProfileNotification::GetProfileNotificationId(
              notification_id, ProfileNotification::GetProfileID(profile))));

  // The pause button should not show because the download is already paused.
  action_buttons = notification_view->GetActionButtonsForTest();
  pause_button_iter = base::ranges::find(action_buttons, pause_button_text,
                                         &views::LabelButton::GetText);
  EXPECT_EQ(pause_button_iter, action_buttons.end());

  // The resume button should show.
  resume_button_iter = base::ranges::find(action_buttons, resume_button_text,
                                          &views::LabelButton::GetText);
  ASSERT_NE(resume_button_iter, action_buttons.end());

  // Implement download resume for the mock client.
  ON_CALL(download_status_updater_client(), Resume(download->guid, _))
      .WillByDefault(
          [&](const std::string& guid,
              crosapi::MockDownloadStatusUpdaterClient::ResumeCallback
                  callback) {
            download->pausable = true;
            download->resumable = false;
            Update(download->Clone());
            std::move(callback).Run(/*handled=*/true);
          });

  // Click on the resume button and wait until download is resumed. Then verify
  // that the click is recorded.
  EXPECT_EQ(tester.GetActionCount("DownloadNotificationV2.Button_Resume"), 0);
  run_loop = std::make_unique<base::RunLoop>();
  EXPECT_CALL(service_observer(), OnNotificationDisplayed)
      .WillOnce(WithArg<0>(
          [&run_loop](const message_center::Notification& notification) {
            run_loop->Quit();
          }));
  test::Click(*resume_button_iter, ui::EF_NONE);
  run_loop->Run();
  Mock::VerifyAndClearExpectations(&service_observer());
  EXPECT_EQ(tester.GetActionCount("DownloadNotificationV2.Button_Pause"), 1);
  EXPECT_EQ(tester.GetActionCount("DownloadNotificationV2.Button_Resume"), 1);
}

// Verifies that the show-in-folder button works as expected.
IN_PROC_BROWSER_TEST_F(NotificationDisplayClientBrowserTest, ShowInFolder) {
  WaitForTestSystemAppInstall();

  // Add a pausable download. Cache the notification ID.
  Profile* const profile = ProfileManager::GetActiveUserProfile();
  std::string notification_id;
  EXPECT_CALL(service_observer(), OnNotificationDisplayed)
      .WillOnce(WithArg<0>(
          [&notification_id](const message_center::Notification& notification) {
            notification_id = notification.id();
          }));
  crosapi::mojom::DownloadStatusPtr download =
      CreateInProgressDownloadStatus(profile,
                                     /*extension=*/"txt",
                                     /*received_bytes=*/0,
                                     /*total_bytes=*/1024);
  Update(download->Clone());
  Mock::VerifyAndClearExpectations(&service_observer());

  // An in-progress notification should not have a show-in-folder button.
  AshNotificationView* popup_view = GetPopupView(profile, notification_id);
  ASSERT_TRUE(popup_view);
  const std::u16string button_text =
      l10n_util::GetStringUTF16(GetCommandTextId(CommandType::kShowInFolder));
  EXPECT_THAT(popup_view->GetActionButtonsForTest(),
              Each(Pointee(Property(&views::LabelButton::GetText,
                                    Not(Eq(button_text))))));

  // Complete the download. Check the existence of the associated notification.
  download->state = crosapi::mojom::DownloadState::kComplete;
  Update(download->Clone());
  EXPECT_THAT(GetDisplayedNotificationIds(), Contains(notification_id));

  // Set up an observer to wait until the Files app opens.
  NiceMock<MockEnvObserver> mock_env_observer;
  base::ScopedObservation<aura::Env, MockEnvObserver> env_observation{
      &mock_env_observer};
  base::RunLoop run_loop;
  ON_CALL(mock_env_observer, OnWindowInitialized)
      .WillByDefault([&run_loop](aura::Window* window) {
        if (aura::test::FindWindowWithTitle(aura::Env::GetInstance(),
                                            u"Files")) {
          run_loop.Quit();
        }
      });

  // Find the show-in-folder button.
  popup_view = GetPopupView(profile, notification_id);
  ASSERT_TRUE(popup_view);
  const auto action_buttons = popup_view->GetActionButtonsForTest();
  auto show_in_folder_button_iter = base::ranges::find(
      action_buttons, button_text, &views::LabelButton::GetText);
  ASSERT_NE(show_in_folder_button_iter, action_buttons.end());

  // Click the show-in-folder button and wait until the Files app opens. Then
  // verify that the click is recorded.
  env_observation.Observe(aura::Env::GetInstance());
  base::UserActionTester tester;
  test::Click(*show_in_folder_button_iter, ui::EF_NONE);
  run_loop.Run();
  EXPECT_EQ(tester.GetActionCount("DownloadNotificationV2.Button_ShowInFolder"),
            1);
}

// Checks the button that enables users to view a download's details in browser.
IN_PROC_BROWSER_TEST_F(NotificationDisplayClientBrowserTest,
                       ViewDownloadDetailsInBrowser) {
  // Create an in-progress download that can be canceled and paused.
  Profile* const profile = ProfileManager::GetActiveUserProfile();
  crosapi::mojom::DownloadStatusPtr download =
      CreateInProgressDownloadStatus(profile,
                                     /*extension=*/"txt",
                                     /*received_bytes=*/0,
                                     /*total_bytes=*/1024);
  download->cancellable = true;
  download->pausable = true;
  download->resumable = false;
  std::string notification_id;
  EXPECT_CALL(service_observer(), OnNotificationDisplayed)
      .WillOnce(WithArg<0>(
          [&notification_id](const message_center::Notification& notification) {
            notification_id = notification.id();
          }));
  Update(download->Clone());
  Mock::VerifyAndClearExpectations(&service_observer());

  AshNotificationView* popup_view = GetPopupView(profile, notification_id);
  ASSERT_TRUE(popup_view);
  const std::u16string button_text = l10n_util::GetStringUTF16(
      GetCommandTextId(CommandType::kViewDetailsInBrowser));

  // Verify that the notification view does not have a "View details in browser"
  // button because `download` is cancelable and pausable.
  EXPECT_THAT(popup_view->GetActionButtonsForTest(),
              Each(Pointee(Property(&views::LabelButton::GetText,
                                    Not(Eq(button_text))))));

  // Update `download` to disable canceling, pausing or resuming. In reality,
  // this could happen when a dangerous download is blocked.
  download->cancellable = false;
  download->pausable = false;
  Update(download->Clone());

  // Find the "View details in browser" button.
  popup_view = GetPopupView(profile, notification_id);
  ASSERT_TRUE(popup_view);
  const auto action_buttons = popup_view->GetActionButtonsForTest();
  auto button_iter = base::ranges::find(action_buttons, button_text,
                                        &views::LabelButton::GetText);
  ASSERT_NE(button_iter, action_buttons.end());

  // Click the "View details in browser" button and wait until showing downloads
  // in browser. Verify that click is recorded.
  base::UserActionTester tester;
  base::RunLoop run_loop;
  EXPECT_CALL(download_status_updater_client(),
              ShowInBrowser(download->guid, _))
      .WillOnce(
          [&run_loop](
              const std::string& guid,
              crosapi::MockDownloadStatusUpdaterClient::ShowInBrowserCallback
                  callback) {
            std::move(callback).Run(/*handled=*/true);
            run_loop.Quit();
          });
  test::Click(*button_iter, ui::EF_NONE);
  run_loop.Run();
  EXPECT_EQ(tester.GetActionCount(
                "DownloadNotificationV2.Button_ViewDetailsInBrowser"),
            1);
}

// NotificationDisplayClientIndeterminateDownloadTest --------------------------

enum class IndeterminateDownloadType {
  kNullTotalByteSize,
  kUnknownTotalByteSize,
};

// Verifies the notification of an indeterminate download works as expected.
class NotificationDisplayClientIndeterminateDownloadTest
    : public NotificationDisplayClientBrowserTest,
      public testing::WithParamInterface<IndeterminateDownloadType> {
 protected:
  std::optional<int64_t> GetTotalByteSize() const {
    switch (GetParam()) {
      case IndeterminateDownloadType::kNullTotalByteSize:
        return std::nullopt;
      case IndeterminateDownloadType::kUnknownTotalByteSize:
        return 0;
    }
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    NotificationDisplayClientIndeterminateDownloadTest,
    testing::Values(IndeterminateDownloadType::kNullTotalByteSize,
                    IndeterminateDownloadType::kUnknownTotalByteSize));

IN_PROC_BROWSER_TEST_P(NotificationDisplayClientIndeterminateDownloadTest,
                       Basics) {
  std::string notification_id;
  EXPECT_CALL(service_observer(), OnNotificationDisplayed)
      .WillOnce(WithArg<0>(
          [&notification_id](const message_center::Notification& notification) {
            notification_id = notification.id();
          }));

  Profile* const profile = ProfileManager::GetActiveUserProfile();
  crosapi::mojom::DownloadStatusPtr download =
      CreateInProgressDownloadStatus(profile,
                                     /*extension=*/"txt",
                                     /*received_bytes=*/0, GetTotalByteSize());

  Update(download->Clone());
  Mock::VerifyAndClearExpectations(&service_observer());

  // Verify that the notification view of an in-progress download has a visible
  // indeterminate progress bar.
  AshNotificationView* popup_view = GetPopupView(profile, notification_id);
  ASSERT_TRUE(popup_view);
  const views::ProgressBar* const progress_bar =
      popup_view->progress_bar_view_for_testing();
  ASSERT_TRUE(progress_bar);
  EXPECT_EQ(progress_bar->GetValue(), -1);
  EXPECT_TRUE(progress_bar->GetVisible());

  // Complete the download. Check the existence of the associated notification.
  MarkDownloadStatusCompleted(*download);
  Update(download->Clone());
  EXPECT_THAT(GetDisplayedNotificationIds(), Contains(notification_id));

  // Verify that the notification view of a completed download does not have
  // a progress bar.
  popup_view = GetPopupView(profile, notification_id);
  ASSERT_TRUE(popup_view);
  EXPECT_FALSE(popup_view->progress_bar_view_for_testing());
}

}  // namespace ash::download_status
