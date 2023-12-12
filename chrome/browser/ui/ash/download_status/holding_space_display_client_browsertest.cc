// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_test_api.h"
#include "ash/public/cpp/holding_space/mock_holding_space_model_observer.h"
#include "ash/test/view_drawn_waiter.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/download_status_updater_ash.h"
#include "chrome/browser/ash/crosapi/mock_download_status_updater_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/ash_test_util.h"
#include "chrome/browser/ui/ash/download_status/display_test_util.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_browsertest_base.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_test_util.h"
#include "chromeos/crosapi/mojom/download_controller.mojom.h"
#include "chromeos/crosapi/mojom/download_status_updater.mojom.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_blocker.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace ash::download_status {

namespace {

// Aliases ---------------------------------------------------------------------

using ::testing::_;
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

}  // namespace

class HoldingSpaceDisplayClientBrowserTest
    : public HoldingSpaceUiBrowserTestBase {
 public:
  HoldingSpaceDisplayClientBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kSysUiDownloadsIntegrationV2);
  }

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
      CreateInProgressDownloadStatus(profile, /*received_bytes=*/0,
                                     /*target_bytes=*/1024);
  in_progress_download->cancellable = true;
  Update(in_progress_download->Clone());
  crosapi::mojom::DownloadStatusPtr completed_download =
      CreateDownloadStatus(profile, crosapi::mojom::DownloadState::kComplete,
                           /*received_bytes=*/1024, /*target_bytes=*/1024);
  Update(completed_download->Clone());
  test_api().Show();

  // Expect two download chips, one for each created download item.
  std::vector<views::View*> download_chips = test_api().GetDownloadChips();
  ASSERT_EQ(download_chips.size(), 2u);

  // Cache download chips. NOTE: Chips are displayed in reverse order of their
  // underlying holding space item creation.
  const views::View* const completed_download_chip = download_chips.at(0);
  const views::View* const in_progress_download_chip = download_chips.at(1);

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
  base::RunLoop run_loop;
  EXPECT_CALL(mock, OnHoldingSpaceItemsRemoved)
      .WillOnce([&](const std::vector<const HoldingSpaceItem*>& items) {
        ASSERT_EQ(items.size(), 1u);
        EXPECT_EQ(items[0]->id(), in_progress_download_id);
        run_loop.Quit();
      });
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  run_loop.Run();

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
      CreateInProgressDownloadStatus(profile, /*received_bytes=*/0,
                                     /*target_bytes=*/1024);
  in_progress_download->cancellable = true;
  Update(in_progress_download->Clone());
  crosapi::mojom::DownloadStatusPtr completed_download =
      CreateDownloadStatus(profile, crosapi::mojom::DownloadState::kComplete,
                           /*received_bytes=*/1024, /*target_bytes=*/1024);
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
  base::RunLoop run_loop;
  EXPECT_CALL(mock, OnHoldingSpaceItemsRemoved)
      .WillOnce([&](const std::vector<const HoldingSpaceItem*>& items) {
        ASSERT_EQ(items.size(), 1u);
        EXPECT_EQ(items[0]->id(), in_progress_download_id);
        run_loop.Quit();
      });
  test::Click(primary_action_container);
  run_loop.Run();

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

IN_PROC_BROWSER_TEST_F(HoldingSpaceDisplayClientBrowserTest, CompleteDownload) {
  Profile* const active_profile = ProfileManager::GetActiveUserProfile();
  crosapi::mojom::DownloadStatusPtr download =
      CreateInProgressDownloadStatus(active_profile, /*received_bytes=*/0,
                                     /*target_bytes=*/1024);
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

  // Cache pointers to the `primary_label` and `secondary_label`.
  const auto* const primary_label = views::AsViewClass<views::Label>(
      cached_download_chip->GetViewByID(kHoldingSpaceItemPrimaryChipLabelId));
  const auto* const secondary_label = views::AsViewClass<views::Label>(
      cached_download_chip->GetViewByID(kHoldingSpaceItemSecondaryChipLabelId));

  // When the target file path is unavailable, the primary text should be the
  // display name of the file referenced by the full path.
  ASSERT_TRUE(download->full_path);
  EXPECT_FALSE(download->target_file_path);
  EXPECT_EQ(primary_label->GetText(),
            download->full_path->BaseName().LossyDisplayName());

  // Check the secondary text.
  EXPECT_TRUE(secondary_label->GetVisible());
  EXPECT_EQ(secondary_label->GetText(), u"0/1,024 B");

  download->target_file_path = CreateFile();
  EXPECT_NE(download->target_file_path, download->full_path);
  Update(download->Clone());

  // When the target file path of an in-progress download item exists, the
  // primary text should be the target file's display name.
  EXPECT_EQ(primary_label->GetText(),
            download->target_file_path->BaseName().LossyDisplayName());

  // Update the received bytes count to half of the total bytes count and
  // then check the progress value.
  download->received_bytes = download->total_bytes.value() / 2.f;
  Update(download->Clone());
  EXPECT_NEAR(item->progress().GetValue().value(), 0.5f,
              std::numeric_limits<float>::epsilon());

  // Check the secondary text.
  EXPECT_EQ(secondary_label->GetText(), u"512/1,024 B");

  // Complete `download`. Verify that the download chip associated to `download`
  // still exists.
  download->received_bytes = download->total_bytes;
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

  // When download is no longer in progress, the `secondary_label` should be
  // hidden.
  EXPECT_FALSE(secondary_label->GetVisible());

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
      CreateInProgressDownloadStatus(active_profile, /*received_bytes=*/0,
                                     /*target_bytes=*/1024);
  duplicate_download->guid = download->guid;
  Update(duplicate_download->Clone());

  // Check that a new download chip is created.
  download_chips = test_api().GetDownloadChips();
  EXPECT_EQ(download_chips.size(), 1u);
}

// Verifies the secondary text in complicated cases.
IN_PROC_BROWSER_TEST_F(HoldingSpaceDisplayClientBrowserTest,
                       ComplicatedSecondaryText) {
  // Create a download status with invalid received and total bytes counts.
  crosapi::mojom::DownloadStatusPtr download = CreateInProgressDownloadStatus(
      ProfileManager::GetActiveUserProfile(), /*received_bytes=*/0,
      /*target_bytes=*/1024);
  download->received_bytes = -1;
  download->total_bytes = 0;
  Update(download->Clone());
  test_api().Show();

  // Verify the existence of a single download chip and cache the secondary
  // label.
  std::vector<views::View*> download_chips = test_api().GetDownloadChips();
  ASSERT_EQ(download_chips.size(), 1u);
  const auto* const secondary_label = views::AsViewClass<views::Label>(
      download_chips[0]->GetViewByID(kHoldingSpaceItemSecondaryChipLabelId));

  // Since both received and total bytes counts are invalid, `secondary_label`
  // should not be visible.
  EXPECT_FALSE(secondary_label->GetVisible());

  // Set the received bytes count of the download to zero. Then check
  // `secondary_label`.
  download->received_bytes = 0;
  Update(download->Clone());
  EXPECT_TRUE(secondary_label->GetVisible());
  EXPECT_EQ(secondary_label->GetText(), u"0 B");

  // Update the received bytes count to another valid value and then check
  // `secondary_label`.
  download->received_bytes = 512;
  Update(download->Clone());
  EXPECT_TRUE(secondary_label->GetVisible());
  EXPECT_EQ(secondary_label->GetText(), u"512 B");

  // Update the in-progress download with a valid total bytes count and then
  // check `secondary_label`.
  download->total_bytes = 1024;
  Update(download->Clone());
  EXPECT_TRUE(secondary_label->GetVisible());
  EXPECT_EQ(secondary_label->GetText(), u"512/1,024 B");
}

IN_PROC_BROWSER_TEST_F(HoldingSpaceDisplayClientBrowserTest,
                       InterruptDownload) {
  crosapi::mojom::DownloadStatusPtr download = CreateInProgressDownloadStatus(
      ProfileManager::GetActiveUserProfile(), /*received_bytes=*/0,
      /*target_bytes=*/1024);
  Update(download->Clone());
  test_api().Show();

  // Verify the existence of a single download chip.
  ASSERT_EQ(test_api().GetDownloadChips().size(), 1u);

  // Interrupt `download`. Verify that the associated download chip is removed.
  download->state = crosapi::mojom::DownloadState::kInterrupted;
  Update(download->Clone());
  EXPECT_TRUE(test_api().GetDownloadChips().empty());
}

// Verifies the behavior when the holding space keyed service is suspended
// during download.
IN_PROC_BROWSER_TEST_F(HoldingSpaceDisplayClientBrowserTest,
                       ServiceSuspendedDuringDownload) {
  crosapi::mojom::DownloadStatusPtr download = CreateInProgressDownloadStatus(
      ProfileManager::GetActiveUserProfile(), /*received_bytes=*/0,
      /*target_bytes=*/1024);
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

}  // namespace ash::download_status
