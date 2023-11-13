// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_test_api.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/download_status_updater_ash.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_browsertest_base.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_test_util.h"
#include "chromeos/crosapi/mojom/download_controller.mojom.h"
#include "chromeos/crosapi/mojom/download_status_updater.mojom.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/view.h"

namespace ash::download_status {

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
  }

  // Updates download through the download status updater.
  void Update(crosapi::mojom::DownloadStatusPtr status) {
    download_status_updater_remote_->Update(std::move(status));
    download_status_updater_remote_.FlushForTesting();
  }

  crosapi::mojom::DownloadStatusPtr CreateDownloadStatus(
      crosapi::mojom::DownloadState state,
      const absl::optional<int64_t>& received_bytes,
      const absl::optional<int64_t>& target_bytes) {
    crosapi::mojom::DownloadStatusPtr download_status =
        crosapi::mojom::DownloadStatus::New();
    download_status->full_path = CreateFile();
    download_status->guid = base::UnguessableToken::Create().ToString();
    download_status->received_bytes = received_bytes;
    download_status->state = state;
    download_status->total_bytes = target_bytes;

    return download_status;
  }

  // Creates a download status that indicates an in progress download.
  crosapi::mojom::DownloadStatusPtr CreateInProgressDownloadStatus() {
    return CreateDownloadStatus(crosapi::mojom::DownloadState::kInProgress,
                                /*received_bytes=*/0, /*target_bytes=*/1024);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode_{
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION};
  mojo::Remote<crosapi::mojom::DownloadStatusUpdater>
      download_status_updater_remote_;
};

IN_PROC_BROWSER_TEST_F(HoldingSpaceDisplayClientBrowserTest, CancelDownload) {
  crosapi::mojom::DownloadStatusPtr download = CreateInProgressDownloadStatus();
  Update(download->Clone());
  test_api().Show();

  // Verify the existence of a single download chip.
  ASSERT_EQ(test_api().GetDownloadChips().size(), 1u);

  // Cancel `download`. Verify that the associated download chip is removed.
  // TODO(http://b/307353486): Cancel the download by UI events when download
  // action handling is implemented.
  download->state = crosapi::mojom::DownloadState::kCancelled;
  Update(download->Clone());
  EXPECT_TRUE(test_api().GetDownloadChips().empty());
}

IN_PROC_BROWSER_TEST_F(HoldingSpaceDisplayClientBrowserTest, CompleteDownload) {
  crosapi::mojom::DownloadStatusPtr download = CreateInProgressDownloadStatus();
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

  // Update the received bytes count to half of the total bytes count and
  // then check the progress value.
  download->received_bytes = download->total_bytes.value() / 2.f;
  Update(download->Clone());
  EXPECT_NEAR(item->progress().GetValue().value(), 0.5f,
              std::numeric_limits<float>::epsilon());

  // Complete `download`. Verify that the download chip associated to `download`
  // still exists.
  download->state = crosapi::mojom::DownloadState::kComplete;
  download->received_bytes = download->total_bytes;
  Update(download->Clone());
  EXPECT_EQ(item->progress().GetValue(), 1.f);
  download_chips = test_api().GetDownloadChips();
  ASSERT_EQ(download_chips.size(), 1u);
  EXPECT_EQ(download_chips[0], cached_download_chip);

  // Remove the download chip.
  Click(download_chips[0]);
  RightClick(download_chips[0]);
  const views::MenuItemView* const menu_item =
      SelectMenuItemWithCommandId(HoldingSpaceCommandId::kRemoveItem);
  ASSERT_TRUE(menu_item);
  Click(menu_item);
  ASSERT_TRUE(test_api().GetDownloadChips().empty());

  // Add a new in-progress download with the duplicate download guid.
  crosapi::mojom::DownloadStatusPtr duplicate_download =
      CreateInProgressDownloadStatus();
  duplicate_download->guid = download->guid;
  Update(duplicate_download->Clone());

  // Check that a new download chip is created.
  download_chips = test_api().GetDownloadChips();
  EXPECT_EQ(download_chips.size(), 1u);
}

IN_PROC_BROWSER_TEST_F(HoldingSpaceDisplayClientBrowserTest,
                       InterruptDownload) {
  crosapi::mojom::DownloadStatusPtr download = CreateInProgressDownloadStatus();
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
  crosapi::mojom::DownloadStatusPtr download = CreateInProgressDownloadStatus();
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
