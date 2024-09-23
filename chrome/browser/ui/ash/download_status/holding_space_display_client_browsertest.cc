// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_metrics.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_test_api.h"
#include "ash/public/cpp/holding_space/mock_holding_space_model_observer.h"
#include "ash/public/cpp/image_util.h"
#include "ash/public/cpp/rounded_image_view.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/system/holding_space/holding_space_item_view.h"
#include "ash/system/progress_indicator/progress_indicator.h"
#include "ash/test/ash_test_util.h"
#include "ash/test/view_drawn_waiter.h"
#include "base/callback_list.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/download_status_updater_ash.h"
#include "chrome/browser/ash/crosapi/mock_download_status_updater_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/download_status/display_test_util.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_browsertest_base.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_test_util.h"
#include "chrome/test/base/ash/util/ash_test_util.h"
#include "chromeos/crosapi/mojom/download_controller.mojom.h"
#include "chromeos/crosapi/mojom/download_status_updater.mojom.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_blocker.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/wm/public/activation_client.h"
#include "ui/wm/public/mock_activation_change_observer.h"

namespace ash::download_status {

namespace {

// Aliases ---------------------------------------------------------------------

using ::testing::_;
using ::testing::Mock;
using ::testing::NiceMock;

// NotificationPopupBlocker ----------------------------------------------------

class NotificationPopupBlocker : public message_center::NotificationBlocker {
 public:
  NotificationPopupBlocker()
      : NotificationBlocker(message_center::MessageCenter::Get()) {}

 private:
  // message_center::NotificationBlocker:
  bool ShouldShowNotificationAsPopup(
      const message_center::Notification& notification) const override {
    return false;
  }
};

// Helpers ---------------------------------------------------------------------

// Creates a holding space icon of `size` with the specified `icon`.
gfx::Image CreateHoldingSpaceIcon(const gfx::ImageSkia& icon,
                                  const gfx::Size& size) {
  return gfx::Image(gfx::ImageSkiaOperations::CreateSuperimposedImage(
      image_util::CreateEmptyImage(size),
      gfx::ImageSkiaOperations::CreateResizedImage(
          icon, skia::ImageOperations::ResizeMethod::RESIZE_GOOD,
          gfx::Size(kHoldingSpaceIconSize, kHoldingSpaceIconSize))));
}

}  // namespace

class HoldingSpaceDisplayClientBrowserTest
    : public HoldingSpaceUiBrowserTestBase {
 public:
  // HoldingSpaceUiBrowserTestBase:
  void SetUpOnMainThread() override {
    HoldingSpaceUiBrowserTestBase::SetUpOnMainThread();

    crosapi::CrosapiManager::Get()->crosapi_ash()->BindDownloadStatusUpdater(
        download_status_updater_remote_.BindNewPipeAndPassReceiver());
    download_status_updater_remote_->BindClient(
        download_status_updater_client_receiver_
            .BindNewPipeAndPassRemoteWithVersion());
    download_status_updater_remote_.FlushForTesting();

    popup_blocker_ = std::make_unique<NotificationPopupBlocker>();
    popup_blocker_->Init();
  }

  void TearDownOnMainThread() override {
    popup_blocker_.reset();
    HoldingSpaceUiBrowserTestBase::TearDownOnMainThread();
  }

  // Updates download through the download status updater.
  void Update(crosapi::mojom::DownloadStatusPtr status) {
    download_status_updater_remote_->Update(std::move(status));
    download_status_updater_remote_.FlushForTesting();
  }

  crosapi::MockDownloadStatusUpdaterClient& download_status_updater_client() {
    return download_status_updater_client_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode_{
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION};

  // Prevents notification popups from hiding the holding space tray.
  std::unique_ptr<NotificationPopupBlocker> popup_blocker_;

  mojo::Remote<crosapi::mojom::DownloadStatusUpdater>
      download_status_updater_remote_;

  // The client bound to the download status updater under test.
  crosapi::MockDownloadStatusUpdaterClient download_status_updater_client_;
  mojo::Receiver<crosapi::mojom::DownloadStatusUpdaterClient>
      download_status_updater_client_receiver_{
          &download_status_updater_client_};
};

IN_PROC_BROWSER_TEST_F(HoldingSpaceDisplayClientBrowserTest,
                       CancelDownloadViaContextMenu) {
  // Create an in-progress download and a completed download.
  Profile* const profile = ProfileManager::GetActiveUserProfile();
  crosapi::mojom::DownloadStatusPtr in_progress_download =
      CreateInProgressDownloadStatus(profile,
                                     /*extension=*/"txt",
                                     /*received_bytes=*/0,
                                     /*total_bytes=*/1024);
  in_progress_download->cancellable = true;
  Update(in_progress_download->Clone());
  crosapi::mojom::DownloadStatusPtr completed_download = CreateDownloadStatus(
      profile, /*extension=*/"txt", crosapi::mojom::DownloadState::kComplete,
      crosapi::mojom::DownloadProgress::New(
          /*loop=*/false,
          /*received_bytes=*/1024,
          /*total_bytes=*/1024,
          /*visible=*/false));
  Update(completed_download->Clone());
  test_api().Show();

  // Expect two download chips, one for each created download item.
  std::vector<views::View*> download_chips = test_api().GetDownloadChips();
  ASSERT_EQ(download_chips.size(), 2u);

  // Cache download chips. NOTE: Chips are displayed in reverse order of their
  // underlying holding space item creation.
  const views::View* const completed_download_chip = download_chips.at(0);
  ASSERT_TRUE(completed_download_chip);
  const views::View* const in_progress_download_chip = download_chips.at(1);
  ASSERT_TRUE(in_progress_download_chip);

  // The image of `completed_download_chip` should show.
  const views::View* const completed_chip_image_view =
      completed_download_chip->GetViewByID(kHoldingSpaceItemImageId);
  ASSERT_TRUE(completed_chip_image_view);
  EXPECT_TRUE(completed_chip_image_view->GetVisible());

  // The image of `in_progress_download_chip` should be hidden.
  const views::View* const in_progress_chip_image_view =
      in_progress_download_chip->GetViewByID(kHoldingSpaceItemImageId);
  ASSERT_TRUE(in_progress_chip_image_view);
  EXPECT_FALSE(in_progress_chip_image_view->GetVisible());

  // Right click the `completed_download_chip`. Because the underlying download
  // is completed, the context menu should not contain a "Cancel" command.
  RightClick(completed_download_chip);
  EXPECT_FALSE(SelectMenuItemWithCommandId(HoldingSpaceCommandId::kCancelItem));

  // Close the context menu and control-right click the
  // `in_progress_download_chip`. Because the `completed_download_chip` is still
  // selected and its underlying download is completed, the context menu should
  // not contain a "Cancel" command.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);
  RightClick(in_progress_download_chip, ui::EF_CONTROL_DOWN);
  EXPECT_FALSE(SelectMenuItemWithCommandId(HoldingSpaceCommandId::kCancelItem));

  // Close the context menu, press the `in_progress_download_chip` and then
  // right click it. Because the `in_progress_download_chip` is the only chip
  // selected and its underlying download is in-progress, the context menu
  // should contain a "Cancel" command.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);
  test::Click(in_progress_download_chip);
  RightClick(in_progress_download_chip);
  EXPECT_TRUE(SelectMenuItemWithCommandId(HoldingSpaceCommandId::kCancelItem));

  // Cache the holding space item IDs associated with the two download chips.
  const std::string completed_download_id =
      test_api().GetHoldingSpaceItemId(completed_download_chip);
  const std::string in_progress_download_id =
      test_api().GetHoldingSpaceItemId(in_progress_download_chip);

  // Bind an observer to watch for updates to the holding space model.
  NiceMock<MockHoldingSpaceModelObserver> mock;
  base::ScopedObservation<HoldingSpaceModel, HoldingSpaceModelObserver>
      observer{&mock};
  observer.Observe(HoldingSpaceController::Get()->model());

  // Implement download cancellation for the mock client.
  ON_CALL(download_status_updater_client(),
          Cancel(in_progress_download->guid, _))
      .WillByDefault(
          [&](const std::string& guid,
              crosapi::MockDownloadStatusUpdaterClient::CancelCallback
                  callback) {
            in_progress_download->state =
                crosapi::mojom::DownloadState::kCancelled;
            Update(std::move(in_progress_download));
            std::move(callback).Run(/*handled=*/true);
          });

  // Press ENTER to execute the "Cancel" command, expecting and waiting for
  // the in-progress download item to be removed from the holding space model.
  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;
  EXPECT_CALL(mock, OnHoldingSpaceItemsRemoved)
      .WillOnce([&](const std::vector<const HoldingSpaceItem*>& items) {
        ASSERT_EQ(items.size(), 1u);
        EXPECT_EQ(items[0]->id(), in_progress_download_id);
        run_loop.Quit();
      });
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  run_loop.Run();
  histogram_tester.ExpectBucketCount("HoldingSpace.Item.Action.All",
                                     holding_space_metrics::ItemAction::kCancel,
                                     /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("HoldingSpace.Item.Action.Cancel",
                                     HoldingSpaceItem::Type::kLacrosDownload,
                                     /*expected_count=*/1);

  // Verify that there is now only a single download chip.
  download_chips = test_api().GetDownloadChips();
  EXPECT_EQ(download_chips.size(), 1u);

  // Because the in-progress download was canceled, only the completed download
  // chip should still be present in the UI.
  EXPECT_TRUE(test_api().GetHoldingSpaceItemView(download_chips,
                                                 completed_download_id));
  EXPECT_FALSE(test_api().GetHoldingSpaceItemView(download_chips,
                                                  in_progress_download_id));
}

IN_PROC_BROWSER_TEST_F(HoldingSpaceDisplayClientBrowserTest,
                       CancelDownloadViaPrimaryAction) {
  // Create an in-progress download and a completed download.
  Profile* const profile = ProfileManager::GetActiveUserProfile();
  crosapi::mojom::DownloadStatusPtr in_progress_download =
      CreateInProgressDownloadStatus(profile,
                                     /*extension=*/"txt",
                                     /*received_bytes=*/0,
                                     /*total_bytes=*/1024);
  in_progress_download->cancellable = true;
  Update(in_progress_download->Clone());
  crosapi::mojom::DownloadStatusPtr completed_download = CreateDownloadStatus(
      profile, /*extension=*/"txt", crosapi::mojom::DownloadState::kComplete,
      crosapi::mojom::DownloadProgress::New(
          /*loop=*/false,
          /*received_bytes=*/1024, /*total_bytes=*/1024, /*visible=*/false));
  Update(completed_download->Clone());
  test_api().Show();

  // Expect two download chips, one for each created download item.
  std::vector<views::View*> download_chips = test_api().GetDownloadChips();
  ASSERT_EQ(download_chips.size(), 2u);

  // Cache download chips. NOTE: Chips are displayed in reverse order of their
  // underlying holding space item creation.
  views::View* const completed_download_chip = download_chips.at(0);
  views::View* const in_progress_download_chip = download_chips.at(1);

  // Hover over the `completed_download_chip`. Because the underlying download
  // is completed, the chip should contain a visible primary action for "Pin".
  test::MoveMouseTo(completed_download_chip, /*count=*/10);
  auto* primary_action_container = completed_download_chip->GetViewByID(
      kHoldingSpaceItemPrimaryActionContainerId);
  const auto* primary_action_cancel =
      primary_action_container->GetViewByID(kHoldingSpaceItemCancelButtonId);
  const auto* primary_action_pin =
      primary_action_container->GetViewByID(kHoldingSpaceItemPinButtonId);
  ViewDrawnWaiter().Wait(primary_action_container);
  EXPECT_FALSE(primary_action_cancel->GetVisible());
  EXPECT_TRUE(primary_action_pin->GetVisible());

  // Hover over the `in_progress_download_chip`. Because the underlying download
  // is in-progress, the chip should contain a visible primary action for
  // "Cancel".
  test::MoveMouseTo(in_progress_download_chip, /*count=*/10);
  primary_action_container = in_progress_download_chip->GetViewByID(
      kHoldingSpaceItemPrimaryActionContainerId);
  primary_action_cancel =
      primary_action_container->GetViewByID(kHoldingSpaceItemCancelButtonId);
  primary_action_pin =
      primary_action_container->GetViewByID(kHoldingSpaceItemPinButtonId);
  ViewDrawnWaiter().Wait(primary_action_container);
  EXPECT_TRUE(primary_action_cancel->GetVisible());
  EXPECT_FALSE(primary_action_pin->GetVisible());

  // Cache the holding space item IDs associated with the two download chips.
  const std::string completed_download_id =
      test_api().GetHoldingSpaceItemId(completed_download_chip);
  const std::string in_progress_download_id =
      test_api().GetHoldingSpaceItemId(in_progress_download_chip);

  // Bind an observer to watch for updates to the holding space model.
  NiceMock<MockHoldingSpaceModelObserver> mock;
  base::ScopedObservation<HoldingSpaceModel, HoldingSpaceModelObserver>
      observer{&mock};
  observer.Observe(HoldingSpaceController::Get()->model());

  // Implement download cancellation for the mock client.
  ON_CALL(download_status_updater_client(),
          Cancel(in_progress_download->guid, _))
      .WillByDefault(
          [&](const std::string& guid,
              crosapi::MockDownloadStatusUpdaterClient::CancelCallback
                  callback) {
            in_progress_download->state =
                crosapi::mojom::DownloadState::kCancelled;
            Update(std::move(in_progress_download));
            std::move(callback).Run(/*handled=*/true);
          });

  // Press the `primary_action_container` to execute "Cancel", expecting and
  // waiting for the in-progress download item to be removed from the holding
  // space model.
  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;
  EXPECT_CALL(mock, OnHoldingSpaceItemsRemoved)
      .WillOnce([&](const std::vector<const HoldingSpaceItem*>& items) {
        ASSERT_EQ(items.size(), 1u);
        EXPECT_EQ(items[0]->id(), in_progress_download_id);
        run_loop.Quit();
      });
  test::Click(primary_action_container);
  run_loop.Run();
  histogram_tester.ExpectBucketCount("HoldingSpace.Item.Action.All",
                                     holding_space_metrics::ItemAction::kCancel,
                                     /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("HoldingSpace.Item.Action.Cancel",
                                     HoldingSpaceItem::Type::kLacrosDownload,
                                     /*expected_count=*/1);

  // Verify that there is now only a single download chip.
  download_chips = test_api().GetDownloadChips();
  EXPECT_EQ(download_chips.size(), 1u);

  // Because the in-progress download was canceled, only the completed download
  // chip should still be present in the UI.
  EXPECT_TRUE(test_api().GetHoldingSpaceItemView(download_chips,
                                                 completed_download_id));
  EXPECT_FALSE(test_api().GetHoldingSpaceItemView(download_chips,
                                                  in_progress_download_id));
}

IN_PROC_BROWSER_TEST_F(HoldingSpaceDisplayClientBrowserTest, OverriddenIcon) {
  DarkLightModeControllerImpl* const dark_light_mode_controller =
      DarkLightModeControllerImpl::Get();
  ASSERT_TRUE(dark_light_mode_controller);

  // Ensure the dark mode enabled.
  if (!dark_light_mode_controller->IsDarkModeEnabled()) {
    dark_light_mode_controller->ToggleColorMode();
  }

  // Create an in-progress download.
  crosapi::mojom::DownloadStatusPtr in_progress_download =
      CreateInProgressDownloadStatus(ProfileManager::GetActiveUserProfile(),
                                     /*extension=*/"txt",
                                     /*received_bytes=*/0,
                                     /*total_bytes=*/1024);

  // Set icons in `in_progress_download` that should override the default
  // holding space icon. NOTE: Use a light/dark colored icon for dark/light
  // modes to improve visibility.
  in_progress_download->icons = crosapi::mojom::DownloadStatusIcons::New(
      /*dark_mode=*/gfx::test::CreateImageSkia(/*size=*/60, SK_ColorWHITE),
      /*light_mode=*/gfx::test::CreateImageSkia(/*size=*/60, SK_ColorBLACK));

  // Hide the download chip's progress indicator.
  in_progress_download->progress->visible = false;

  // Without using a zero delay, the process of loading bitmap from the download
  // file, which returns a null bitmap as expected but notifiers image change
  // waiters, could complete before the invalidation timer fires. Therefore, use
  // a zero delay to avoid flakiness.
  HoldingSpaceImage::SetUseZeroInvalidationDelayForTesting(true);

  Update(in_progress_download->Clone());
  test_api().Show();

  // Cache `holding_space_image`.
  std::vector<views::View*> download_chips = test_api().GetDownloadChips();
  ASSERT_EQ(download_chips.size(), 1u);
  const auto* item_view =
      views::AsViewClass<HoldingSpaceItemView>(download_chips.at(0));
  const HoldingSpaceImage& holding_space_image =
      item_view->item()->image_for_testing();

  // Wait until `holding_space_image` is ready.
  base::test::TestFuture<void> test_future;
  auto subscription = holding_space_image.AddImageSkiaChangedCallback(
      test_future.GetRepeatingCallback());
  ASSERT_TRUE(test_future.Wait());
  HoldingSpaceImage::SetUseZeroInvalidationDelayForTesting(false);

  // Verify the image for the dark mode.
  const auto* const image_view = views::AsViewClass<RoundedImageView>(
      item_view->GetViewByID(kHoldingSpaceItemImageId));
  const crosapi::mojom::DownloadStatusIconsPtr& icons =
      in_progress_download->icons;
  constexpr gfx::Size kIconSize(kHoldingSpaceChipIconSize,
                                kHoldingSpaceChipIconSize);
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      gfx::Image(image_view->original_image()),
      CreateHoldingSpaceIcon(icons->dark_mode, kIconSize)));

  // Switch to the light mode. Then verify the image.
  dark_light_mode_controller->ToggleColorMode();
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      gfx::Image(image_view->original_image()),
      CreateHoldingSpaceIcon(icons->light_mode, kIconSize)));
}

// Verifies clicking a completed download's holding space chip.
IN_PROC_BROWSER_TEST_F(HoldingSpaceDisplayClientBrowserTest,
                       ClickCompletedDownloadChip) {
  // Add a completed download.
  crosapi::mojom::DownloadStatusPtr download = CreateDownloadStatus(
      ProfileManager::GetActiveUserProfile(),
      /*extension=*/"txt", crosapi::mojom::DownloadState::kComplete,
      crosapi::mojom::DownloadProgress::New(
          /*loop=*/false,
          /*received_bytes=*/1024,
          /*total_bytes=*/1024,
          /*visible=*/false));
  Update(download->Clone());
  test_api().Show();

  // Cache `completed_download_chip`.
  std::vector<views::View*> download_chips = test_api().GetDownloadChips();
  ASSERT_EQ(download_chips.size(), 1u);
  views::View* const completed_download_chip = download_chips.at(0);

  // Observe the `activation_client` so we can detect windows becoming active as
  // a result of opening the download file.
  NiceMock<MockActivationChangeObserver> activation_mock_observer;
  base::ScopedObservation<wm::ActivationClient, wm::ActivationChangeObserver>
      activation_observation{&activation_mock_observer};
  auto* const activation_client = wm::GetActivationClient(
      completed_download_chip->GetWidget()->GetNativeView()->GetRootWindow());
  ASSERT_TRUE(activation_client);
  activation_observation.Observe(activation_client);

  // The command that shows downloads in browser should not be performed.
  EXPECT_CALL(download_status_updater_client(), ShowInBrowser).Times(0);

  // Double click `completed_download_chip` and then wait until window
  // activation updates. Minimize the browser window before click to ensure
  // the window activation change.
  WaitForTestSystemAppInstall();
  browser()->window()->Minimize();
  base::RunLoop run_loop;
  EXPECT_CALL(activation_mock_observer, OnWindowActivated)
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  test::Click(completed_download_chip, ui::EF_IS_DOUBLE_CLICK);
  run_loop.Run();
  Mock::VerifyAndClearExpectations(&activation_mock_observer);
  Mock::VerifyAndClearExpectations(&download_status_updater_client());
}

// Verifies clicking an in-progress download's holding space chip.
IN_PROC_BROWSER_TEST_F(HoldingSpaceDisplayClientBrowserTest,
                       ClickInProgressDownloadChip) {
  // Add an in-progress download.
  crosapi::mojom::DownloadStatusPtr download =
      CreateInProgressDownloadStatus(ProfileManager::GetActiveUserProfile(),
                                     /*extension=*/"txt",
                                     /*received_bytes=*/0,
                                     /*total_bytes=*/1024);
  Update(download->Clone());
  test_api().Show();

  // Cache `in_progress_download_chip`.
  std::vector<views::View*> download_chips = test_api().GetDownloadChips();
  ASSERT_EQ(download_chips.size(), 1u);
  views::View* const in_progress_download_chip = download_chips.at(0);

  // Double click `in_progress_download_chip`. Check that the underlying
  // download is shown in browser.
  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;
  EXPECT_CALL(download_status_updater_client(),
              ShowInBrowser(download->guid, _))
      .WillOnce(
          [&run_loop](
              const std::string& guid,
              crosapi::mojom::DownloadStatusUpdaterClient::ShowInBrowserCallback
                  callback) {
            std::move(callback).Run(/*handled=*/true);
            run_loop.Quit();
          });
  test::Click(in_progress_download_chip, ui::EF_IS_DOUBLE_CLICK);
  run_loop.Run();
  Mock::VerifyAndClearExpectations(&download_status_updater_client());
  histogram_tester.ExpectBucketCount(
      "HoldingSpace.Item.Action.All",
      holding_space_metrics::ItemAction::kShowInBrowser,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("HoldingSpace.Item.Action.ShowInBrowser",
                                     HoldingSpaceItem::Type::kLacrosDownload,
                                     /*expected_count=*/1);
}

IN_PROC_BROWSER_TEST_F(HoldingSpaceDisplayClientBrowserTest, CompleteDownload) {
  Profile* const active_profile = ProfileManager::GetActiveUserProfile();
  crosapi::mojom::DownloadStatusPtr download =
      CreateInProgressDownloadStatus(active_profile,
                                     /*extension=*/"txt",
                                     /*received_bytes=*/0,
                                     /*total_bytes=*/1024);
  Update(download->Clone());
  test_api().Show();

  // Verify the existence of a single download chip and cache the chip.
  std::vector<views::View*> download_chips = test_api().GetDownloadChips();
  ASSERT_EQ(download_chips.size(), 1u);
  const views::View* const cached_download_chip = download_chips[0];

  // Check the holding space item's progress value when download starts.
  const HoldingSpaceItem* const item =
      HoldingSpaceController::Get()->model()->GetItem(
          test_api().GetHoldingSpaceItemId(cached_download_chip));
  EXPECT_EQ(item->progress().GetValue(), 0.f);

  // Cache the primary label.
  const auto* const primary_label = views::AsViewClass<views::Label>(
      cached_download_chip->GetViewByID(kHoldingSpaceItemPrimaryChipLabelId));

  // When the target file path is unavailable, the primary text should be the
  // display name of the file referenced by the full path.
  ASSERT_TRUE(download->full_path);
  EXPECT_FALSE(download->target_file_path);
  EXPECT_EQ(primary_label->GetText(),
            download->full_path->BaseName().LossyDisplayName());

  download->target_file_path = CreateFile();
  EXPECT_NE(download->target_file_path, download->full_path);
  Update(download->Clone());

  // When the target file path of an in-progress download item exists, the
  // primary text should be the target file's display name.
  EXPECT_EQ(primary_label->GetText(),
            download->target_file_path->BaseName().LossyDisplayName());

  // Update the received bytes count to half of the total bytes count and
  // then check the progress value.
  crosapi::mojom::DownloadProgressPtr& progress = download->progress;
  progress->received_bytes = progress->total_bytes / 2.f;
  Update(download->Clone());
  EXPECT_NEAR(item->progress().GetValue().value(), 0.5f,
              std::numeric_limits<float>::epsilon());

  // Update the path to the file being written to during download. Check the
  // holding space item's backing file path after update.
  const base::FilePath old_file_path = item->file().file_path;
  download->full_path = CreateFile();
  EXPECT_NE(download->full_path, old_file_path);
  Update(download->Clone());
  EXPECT_EQ(item->file().file_path, download->full_path.value());

  // Complete `download`. Verify that the download chip associated to `download`
  // still exists.
  progress->received_bytes = progress->total_bytes;
  download->state = crosapi::mojom::DownloadState::kComplete;
  Update(download->Clone());
  EXPECT_EQ(item->progress().GetValue(), 1.f);
  download_chips = test_api().GetDownloadChips();
  ASSERT_EQ(download_chips.size(), 1u);
  EXPECT_EQ(download_chips[0], cached_download_chip);

  // A completed download item's primary text should be the display name of the
  // file referenced by the full path.
  EXPECT_EQ(primary_label->GetText(),
            download->full_path->BaseName().LossyDisplayName());

  // Remove the download chip.
  test::Click(download_chips[0]);
  RightClick(download_chips[0]);
  const views::MenuItemView* const menu_item =
      SelectMenuItemWithCommandId(HoldingSpaceCommandId::kRemoveItem);
  ASSERT_TRUE(menu_item);
  test::Click(menu_item);
  ASSERT_TRUE(test_api().GetDownloadChips().empty());

  // Add a new in-progress download with the duplicate download guid.
  crosapi::mojom::DownloadStatusPtr duplicate_download =
      CreateInProgressDownloadStatus(active_profile,
                                     /*extension=*/"txt",
                                     /*received_bytes=*/0,
                                     /*total_bytes=*/1024);
  duplicate_download->guid = download->guid;
  Update(duplicate_download->Clone());

  // Check that a new download chip is created.
  download_chips = test_api().GetDownloadChips();
  EXPECT_EQ(download_chips.size(), 1u);
}

IN_PROC_BROWSER_TEST_F(HoldingSpaceDisplayClientBrowserTest,
                       InProgressDownloadWithHiddenProgress) {
  // Create an in-progress download with an invisible progress. In reality, this
  // could happen when a download is blocked.
  crosapi::mojom::DownloadStatusPtr download =
      CreateInProgressDownloadStatus(ProfileManager::GetActiveUserProfile(),
                                     /*extension=*/"txt",
                                     /*received_bytes=*/0,
                                     /*total_bytes=*/1024);
  download->progress->visible = false;
  Update(download->Clone());
  test_api().Show();

  // Verify the existence of a single download chip.
  const std::vector<views::View*> chips = test_api().GetDownloadChips();
  ASSERT_EQ(chips.size(), 1u);
  views::View* in_progress_download_chip = chips[0];
  ASSERT_TRUE(in_progress_download_chip);

  // The image of `in_progress_download_chip` should show because `download`
  // suggests that the progress is hidden.
  const views::View* const image_view =
      in_progress_download_chip->GetViewByID(kHoldingSpaceItemImageId);
  ASSERT_TRUE(image_view);
  EXPECT_TRUE(image_view->GetVisible());
}

IN_PROC_BROWSER_TEST_F(HoldingSpaceDisplayClientBrowserTest,
                       InterruptDownload) {
  crosapi::mojom::DownloadStatusPtr download =
      CreateInProgressDownloadStatus(ProfileManager::GetActiveUserProfile(),
                                     /*extension=*/"txt",
                                     /*received_bytes=*/0,
                                     /*total_bytes=*/1024);
  Update(download->Clone());
  test_api().Show();

  // Verify the existence of a single download chip.
  ASSERT_EQ(test_api().GetDownloadChips().size(), 1u);

  // Interrupt `download`. Verify that the associated download chip is removed.
  download->state = crosapi::mojom::DownloadState::kInterrupted;
  Update(download->Clone());
  EXPECT_TRUE(test_api().GetDownloadChips().empty());
}

IN_PROC_BROWSER_TEST_F(HoldingSpaceDisplayClientBrowserTest,
                       PauseAndResumeDownloadViaContextMenu) {
  crosapi::mojom::DownloadStatusPtr download =
      CreateInProgressDownloadStatus(ProfileManager::GetActiveUserProfile(),
                                     /*extension=*/"txt",
                                     /*received_bytes=*/0,
                                     /*total_bytes=*/1024);
  download->pausable = true;
  Update(download->Clone());
  test_api().Show();

  // Verify the existence of a single download chip.
  const std::vector<views::View*> download_chips =
      test_api().GetDownloadChips();
  ASSERT_EQ(download_chips.size(), 1u);

  // Right click the download chip. Because the underlying download is in
  // progress, the context menu should contain a "Pause" command.
  RightClick(download_chips[0]);
  EXPECT_TRUE(SelectMenuItemWithCommandId(HoldingSpaceCommandId::kPauseItem));

  // Press ENTER to execute the "Pause" command and then check that the download
  // is paused.
  base::HistogramTester histogram_tester;
  auto run_loop = std::make_unique<base::RunLoop>();
  EXPECT_CALL(download_status_updater_client(), Pause(download->guid, _))
      .WillOnce([&](const std::string& guid,
                    crosapi::MockDownloadStatusUpdaterClient::PauseCallback
                        callback) {
        download->pausable = false;
        download->resumable = true;
        Update(download->Clone());
        std::move(callback).Run(/*handled=*/true);
        run_loop->Quit();
      });
  PressAndReleaseKey(ui::VKEY_RETURN);
  run_loop->Run();
  histogram_tester.ExpectBucketCount("HoldingSpace.Item.Action.All",
                                     holding_space_metrics::ItemAction::kPause,
                                     /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("HoldingSpace.Item.Action.Pause",
                                     HoldingSpaceItem::Type::kLacrosDownload,
                                     /*expected_count=*/1);

  // Right click the download chip. Because the underlying download is paused,
  // the context menu should contain a "Resume" command.
  RightClick(download_chips[0]);
  EXPECT_TRUE(SelectMenuItemWithCommandId(HoldingSpaceCommandId::kResumeItem));

  // Download resuming should not be recorded yet.
  histogram_tester.ExpectBucketCount("HoldingSpace.Item.Action.All",
                                     holding_space_metrics::ItemAction::kResume,
                                     /*expected_count=*/0);
  histogram_tester.ExpectBucketCount("HoldingSpace.Item.Action.Resume",
                                     HoldingSpaceItem::Type::kLacrosDownload,
                                     /*expected_count=*/0);

  // Press ENTER to execute the "Resume" command and then check that the
  // download is resumed.
  run_loop = std::make_unique<base::RunLoop>();
  EXPECT_CALL(download_status_updater_client(), Resume(download->guid, _))
      .WillOnce([&](const std::string& guid,
                    crosapi::MockDownloadStatusUpdaterClient::ResumeCallback
                        callback) {
        download->pausable = true;
        download->resumable = false;
        Update(download->Clone());
        std::move(callback).Run(/*handled=*/true);
        run_loop->Quit();
      });
  PressAndReleaseKey(ui::VKEY_RETURN);
  run_loop->Run();
  histogram_tester.ExpectBucketCount("HoldingSpace.Item.Action.All",
                                     holding_space_metrics::ItemAction::kResume,
                                     /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("HoldingSpace.Item.Action.Resume",
                                     HoldingSpaceItem::Type::kLacrosDownload,
                                     /*expected_count=*/1);
}

IN_PROC_BROWSER_TEST_F(HoldingSpaceDisplayClientBrowserTest,
                       PauseAndResumeDownloadViaSecondaryAction) {
  crosapi::mojom::DownloadStatusPtr download =
      CreateInProgressDownloadStatus(ProfileManager::GetActiveUserProfile(),
                                     /*extension=*/"txt",
                                     /*received_bytes=*/0,
                                     /*total_bytes=*/1024);
  download->pausable = true;
  Update(download->Clone());
  test_api().Show();

  // Verify the existence of a single download chip.
  const std::vector<views::View*> download_chips =
      test_api().GetDownloadChips();
  ASSERT_EQ(download_chips.size(), 1u);
  views::View* const download_chip = download_chips[0];

  // Move mouse to `download_chip` and then wait until `pause_button` shows.
  test::MoveMouseTo(download_chip, /*count=*/10);
  auto* const secondary_action_container =
      download_chip->GetViewByID(kHoldingSpaceItemSecondaryActionContainerId);
  ASSERT_TRUE(secondary_action_container);
  auto* const pause_button =
      secondary_action_container->GetViewByID(kHoldingSpaceItemPauseButtonId);
  ASSERT_TRUE(pause_button);
  ViewDrawnWaiter().Wait(pause_button);

  // Press `pause_button` and then check that the download is paused.
  base::HistogramTester histogram_tester;
  auto run_loop = std::make_unique<base::RunLoop>();
  EXPECT_CALL(download_status_updater_client(), Pause(download->guid, _))
      .WillOnce([&](const std::string& guid,
                    crosapi::MockDownloadStatusUpdaterClient::PauseCallback
                        callback) {
        download->pausable = false;
        download->resumable = true;
        Update(download->Clone());
        std::move(callback).Run(/*handled=*/true);
        run_loop->Quit();
      });
  test::Click(pause_button);
  run_loop->Run();
  histogram_tester.ExpectBucketCount("HoldingSpace.Item.Action.All",
                                     holding_space_metrics::ItemAction::kPause,
                                     /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("HoldingSpace.Item.Action.Pause",
                                     HoldingSpaceItem::Type::kLacrosDownload,
                                     /*expected_count=*/1);

  // Move mouse to `download_chip` and wait until `resume_button` shows.
  test::MoveMouseTo(download_chip, /*count=*/10);
  auto* const resume_button =
      secondary_action_container->GetViewByID(kHoldingSpaceItemResumeButtonId);
  ASSERT_TRUE(resume_button);
  ViewDrawnWaiter().Wait(resume_button);

  // Download resuming should not be recorded yet.
  histogram_tester.ExpectBucketCount("HoldingSpace.Item.Action.All",
                                     holding_space_metrics::ItemAction::kResume,
                                     /*expected_count=*/0);
  histogram_tester.ExpectBucketCount("HoldingSpace.Item.Action.Resume",
                                     HoldingSpaceItem::Type::kLacrosDownload,
                                     /*expected_count=*/0);

  // Press `resume_button` and then check that the download is resumed.
  run_loop = std::make_unique<base::RunLoop>();
  EXPECT_CALL(download_status_updater_client(), Resume(download->guid, _))
      .WillOnce([&](const std::string& guid,
                    crosapi::MockDownloadStatusUpdaterClient::ResumeCallback
                        callback) {
        download->pausable = true;
        download->resumable = false;
        Update(download->Clone());
        std::move(callback).Run(/*handled=*/true);
        run_loop->Quit();
      });
  test::Click(resume_button);
  run_loop->Run();
  histogram_tester.ExpectBucketCount("HoldingSpace.Item.Action.All",
                                     holding_space_metrics::ItemAction::kResume,
                                     /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("HoldingSpace.Item.Action.Resume",
                                     HoldingSpaceItem::Type::kLacrosDownload,
                                     /*expected_count=*/1);
}

IN_PROC_BROWSER_TEST_F(HoldingSpaceDisplayClientBrowserTest, SecondaryLabel) {
  crosapi::mojom::DownloadStatusPtr download =
      CreateInProgressDownloadStatus(ProfileManager::GetActiveUserProfile(),
                                     /*extension=*/"txt",
                                     /*received_bytes=*/0,
                                     /*total_bytes=*/1024);
  Update(download->Clone());
  test_api().Show();

  // Cache the secondary label.
  std::vector<views::View*> download_chips = test_api().GetDownloadChips();
  ASSERT_EQ(download_chips.size(), 1u);
  const auto* const secondary_label = views::AsViewClass<views::Label>(
      download_chips[0]->GetViewByID(kHoldingSpaceItemSecondaryChipLabelId));

  // `download` does not specify the status text. Therefore, `secondary_label`
  // should not show.
  EXPECT_FALSE(secondary_label->GetVisible());

  // Set the status text of `download` and then check `secondary_label`.
  download->status_text = u"random text";
  Update(download->Clone());
  EXPECT_TRUE(secondary_label->GetVisible());
  EXPECT_EQ(secondary_label->GetText(), u"random text");

  // Set the status text with an empty string and then check `secondary_label`.
  download->status_text = std::u16string();
  Update(download->Clone());
  EXPECT_FALSE(secondary_label->GetVisible());
}

// Verifies the behavior when the holding space keyed service is suspended
// during download.
IN_PROC_BROWSER_TEST_F(HoldingSpaceDisplayClientBrowserTest,
                       ServiceSuspendedDuringDownload) {
  crosapi::mojom::DownloadStatusPtr download =
      CreateInProgressDownloadStatus(ProfileManager::GetActiveUserProfile(),
                                     /*extension=*/"txt",
                                     /*received_bytes=*/0,
                                     /*total_bytes=*/1024);
  Update(download->Clone());
  test_api().Show();

  // Cache the holding space item ID.
  std::vector<views::View*> download_chips = test_api().GetDownloadChips();
  ASSERT_EQ(download_chips.size(), 1u);
  const std::string item_id =
      test_api().GetHoldingSpaceItemId(download_chips[0]);

  // Suspend the service. Wait until the item specified by `item_id` is removed.
  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
  WaitForItemRemovalById(item_id);

  // Check that a download update during suspension does not create a new item.
  // Use a different file path to prevent the new item, if any, from being
  // filtered out due to duplication.
  download->full_path = CreateFile();
  Update(download->Clone());
  EXPECT_TRUE(HoldingSpaceController::Get()->model()->items().empty());

  // End suspension. The holding space model should be empty. Since the download
  // is in progress, its associated holding space item is not persistent.
  chromeos::FakePowerManagerClient::Get()->SendSuspendDone();
  EXPECT_TRUE(HoldingSpaceController::Get()->model()->items().empty());

  // Update the download after suspension. A new holding space item should be
  // created.
  Update(download->Clone());
  EXPECT_EQ(HoldingSpaceController::Get()->model()->items().size(), 1u);
  download_chips = test_api().GetDownloadChips();
  ASSERT_EQ(download_chips.size(), 1u);
  EXPECT_NE(test_api().GetHoldingSpaceItemId(download_chips[0]), item_id);
}

// Verifies viewing a download's details in browser via context menu.
IN_PROC_BROWSER_TEST_F(HoldingSpaceDisplayClientBrowserTest,
                       ViewDownloadDetailsInBrowser) {
  // Create an in-progress download that can be canceled and paused.
  crosapi::mojom::DownloadStatusPtr download =
      CreateInProgressDownloadStatus(ProfileManager::GetActiveUserProfile(),
                                     /*extension=*/"txt",
                                     /*received_bytes=*/0,
                                     /*total_bytes=*/1024);
  download->cancellable = true;
  download->pausable = true;
  download->resumable = false;
  Update(download->Clone());
  test_api().Show();

  // Verify the existence of a single download chip.
  std::vector<views::View*> download_chips = test_api().GetDownloadChips();
  ASSERT_EQ(download_chips.size(), 1u);

  // Right click the download chip. Because `download` is cancelable and
  // pausable, the context menu should not contain "View details in browser".
  RightClick(download_chips[0]);
  EXPECT_FALSE(SelectMenuItemWithCommandId(
      HoldingSpaceCommandId::kViewItemDetailsInBrowser));

  // Close the context menu.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);

  // Update `download` to disable canceling, pausing or resuming. In reality,
  // this could happen when a dangerous download is blocked.
  download->cancellable = false;
  download->pausable = false;
  Update(download->Clone());

  // Right click the download chip. Verify that the context menu contains a
  // "View details in browser" command.
  download_chips = test_api().GetDownloadChips();
  ASSERT_EQ(download_chips.size(), 1u);
  RightClick(download_chips[0]);
  EXPECT_TRUE(SelectMenuItemWithCommandId(
      HoldingSpaceCommandId::kViewItemDetailsInBrowser));

  // Press ENTER to execute the "View details in browser" command. Then check
  // that the download is shown in browser.
  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;
  EXPECT_CALL(download_status_updater_client(),
              ShowInBrowser(download->guid, _))
      .WillOnce(
          [&](const std::string& guid,
              crosapi::MockDownloadStatusUpdaterClient::ShowInBrowserCallback
                  callback) {
            std::move(callback).Run(/*handled=*/true);
            run_loop.Quit();
          });
  PressAndReleaseKey(ui::VKEY_RETURN);
  run_loop.Run();
  histogram_tester.ExpectBucketCount(
      "HoldingSpace.Item.Action.All",
      holding_space_metrics::ItemAction::kViewDetailsInBrowser,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "HoldingSpace.Item.Action.ViewDetailsInBrowser",
      HoldingSpaceItem::Type::kLacrosDownload,
      /*expected_count=*/1);
}

// HoldingSpaceDisplayClientIndeterminateDownloadTest --------------------------

enum class IndeterminateDownloadType {
  kNullTotalByteSize,
  kUnknownTotalByteSize,
};

// Verifies that holding space works as expected with indeterminate downloads.
class HoldingSpaceDisplayClientIndeterminateDownloadTest
    : public HoldingSpaceDisplayClientBrowserTest,
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
    HoldingSpaceDisplayClientIndeterminateDownloadTest,
    testing::Values(IndeterminateDownloadType::kNullTotalByteSize,
                    IndeterminateDownloadType::kUnknownTotalByteSize));

IN_PROC_BROWSER_TEST_P(HoldingSpaceDisplayClientIndeterminateDownloadTest,
                       Basics) {
  crosapi::mojom::DownloadStatusPtr download = CreateInProgressDownloadStatus(
      ProfileManager::GetActiveUserProfile(),
      /*extension=*/"txt", /*received_bytes=*/0, GetTotalByteSize());
  Update(download->Clone());
  test_api().Show();

  // Verify the existence of a single download chip.
  std::vector<views::View*> const chips = test_api().GetDownloadChips();
  ASSERT_EQ(chips.size(), 1u);

  ProgressIndicator* const progress_indicator = static_cast<ProgressIndicator*>(
      FindLayerWithName(chips[0], ProgressIndicator::kClassName)->owner());
  ASSERT_TRUE(progress_indicator);

  // Wait until `progress_indicator` updates.
  base::test::TestFuture<void> progress_change_waiter;
  base::CallbackListSubscription subscription =
      progress_indicator->AddProgressChangedCallback(
          progress_change_waiter.GetRepeatingCallback());
  ASSERT_TRUE(progress_change_waiter.Wait());

  // The progress indicator should suggest an indeterminate download.
  EXPECT_EQ(progress_indicator->progress(), std::nullopt);

  // Complete the download and check the existence of the download chip.
  download->state = crosapi::mojom::DownloadState::kComplete;
  Update(download->Clone());
  EXPECT_EQ(test_api().GetDownloadChips().size(), 1u);
}

}  // namespace ash::download_status
