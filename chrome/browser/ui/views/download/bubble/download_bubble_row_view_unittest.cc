// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_bubble_row_view.h"

#include "chrome/browser/download/bubble/download_bubble_ui_controller.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_navigation_handler.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/download_item_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/clipboard/test/clipboard_test_util.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/test_event.h"
#include "ui/events/types/event_type.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/test/mock_input_event_activation_protector.h"

namespace {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::ReturnRefOfCopy;

constexpr int kTimeSinceDownloadCompletedUpdateSeconds = 60;

class MockDownloadBubbleNavigationHandler
    : public DownloadBubbleNavigationHandler {
 public:
  virtual ~MockDownloadBubbleNavigationHandler() = default;
  void OpenPrimaryDialog() override {}
  void OpenSecurityDialog(const offline_items_collection::ContentId&) override {
  }
  void CloseDialog(views::Widget::ClosedReason) override {}
  MOCK_METHOD(void,
              OnSecurityDialogButtonPress,
              (const DownloadUIModel& model, DownloadCommands::Command command),
              (override));
  void OnDialogInteracted() override {}
  std::unique_ptr<views::BubbleDialogDelegate::CloseOnDeactivatePin>
  PreventDialogCloseOnDeactivate() override {
    return nullptr;
  }
  base::WeakPtr<DownloadBubbleNavigationHandler> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockDownloadBubbleNavigationHandler> weak_factory_{this};
};

class DownloadBubbleRowViewTest : public ChromeViewsTestBase {
 public:
  DownloadBubbleRowViewTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  DownloadBubbleRowViewTest(const DownloadBubbleRowViewTest&) = delete;
  DownloadBubbleRowViewTest& operator=(const DownloadBubbleRowViewTest&) =
      delete;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    profile_ = testing_profile_manager_.CreateTestingProfile("testing_profile");

    content::DownloadItemUtils::AttachInfoForTesting(&download_item_, profile_,
                                                     nullptr);
    ON_CALL(download_item_, GetURL())
        .WillByDefault(ReturnRef(GURL::EmptyGURL()));

    EXPECT_CALL(mock_browser_window_interface_, GetProfile())
        .WillRepeatedly(Return(profile_));
    bubble_controller_ = std::make_unique<DownloadBubbleUIController>(
        &mock_browser_window_interface_);
    navigation_handler_ =
        std::make_unique<MockDownloadBubbleNavigationHandler>();

    const int bubble_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
        views::DISTANCE_BUBBLE_PREFERRED_WIDTH);
    info_ = std::make_unique<DownloadBubbleRowViewInfo>(DownloadItemModel::Wrap(
        &download_item_,
        std::make_unique<DownloadUIModel::BubbleStatusTextBuilder>()));
    row_view_ = std::make_unique<DownloadBubbleRowView>(
        *info_, bubble_controller_->GetWeakPtr(),
        navigation_handler_->GetWeakPtr(), nullptr, bubble_width);

    auto input_protector =
        std::make_unique<NiceMock<views::MockInputEventActivationProtector>>();
    input_protector_ = input_protector.get();
    ON_CALL(*input_protector_, IsPossiblyUnintendedInteraction(_, _, _))
        .WillByDefault(Return(false));
    row_view_->SetInputProtectorForTesting(std::move(input_protector));
  }

  void FastForward(base::TimeDelta time) {
    task_environment()->FastForwardBy(time);
  }

  DownloadBubbleRowView* row_view() { return row_view_.get(); }
  download::MockDownloadItem* download_item() { return &download_item_; }

 protected:
  TestingProfileManager testing_profile_manager_;
  raw_ptr<Profile> profile_ = nullptr;
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_interface_;
  std::unique_ptr<DownloadBubbleUIController> bubble_controller_;
  std::unique_ptr<MockDownloadBubbleNavigationHandler> navigation_handler_;
  NiceMock<download::MockDownloadItem> download_item_;
  std::unique_ptr<DownloadBubbleRowViewInfo> info_;
  std::unique_ptr<DownloadBubbleRowView> row_view_;
  raw_ptr<NiceMock<views::MockInputEventActivationProtector>> input_protector_;
};

TEST_F(DownloadBubbleRowViewTest, CopyAcceleratorCopiesFile) {
#if BUILDFLAG(IS_WIN)
  base::FilePath target_path(FILE_PATH_LITERAL("\\test.exe"));
#else
  base::FilePath target_path(FILE_PATH_LITERAL("/test.exe"));
#endif
  ON_CALL(*download_item(), GetState())
      .WillByDefault(Return(download::DownloadItem::COMPLETE));
  ON_CALL(*download_item(), GetTargetFilePath())
      .WillByDefault(ReturnRefOfCopy(target_path));

  ui::TestClipboard* clipboard = ui::TestClipboard::CreateForCurrentThread();

#if BUILDFLAG(IS_MAC)
  int modifiers = ui::EF_COMMAND_DOWN;
#else
  int modifiers = ui::EF_CONTROL_DOWN;
#endif
  ui::Accelerator accelerator(ui::VKEY_C, modifiers);

  row_view()->AcceleratorPressed(accelerator);
  std::vector<ui::FileInfo> filenames = ui::clipboard_test_util::ReadFilenames(
      clipboard, ui::ClipboardBuffer::kCopyPaste, nullptr);
  ASSERT_EQ(filenames.size(), 1u);
  EXPECT_EQ(filenames[0].path, target_path);

  clipboard->DestroyClipboardForCurrentThread();
}

TEST_F(DownloadBubbleRowViewTest, UpdateTimeFromCompletedDownload) {
  ON_CALL(*download_item(), GetState())
      .WillByDefault(Return(download::DownloadItem::COMPLETE));
  ON_CALL(*download_item(), GetEndTime())
      .WillByDefault(Return(base::Time::Now()));
  download_item()->NotifyObserversDownloadUpdated();
  // Get starting label for a finished download and ensure it stays
  // the same until one timer interval.
  std::u16string row_label(row_view()->GetSecondaryLabelTextForTesting());
  FastForward(base::Seconds(kTimeSinceDownloadCompletedUpdateSeconds - 1));
  EXPECT_EQ(row_label, row_view()->GetSecondaryLabelTextForTesting());
  // After a timer interval, check to make sure that the label has
  // changed.
  FastForward(base::Seconds(kTimeSinceDownloadCompletedUpdateSeconds));
  EXPECT_NE(row_label, row_view()->GetSecondaryLabelTextForTesting());
}

TEST_F(DownloadBubbleRowViewTest, MainButtonPressed) {
  EXPECT_CALL(*download_item(), OpenDownload()).Times(1);
  row_view()->SimulateMainButtonClickForTesting(ui::test::TestEvent());
}

// Tests that only enabled quick actions that are in the `ui_info_` are visible
// on the row view.
TEST_F(DownloadBubbleRowViewTest, OnlyEnabledQuickActionsVisible) {
  ON_CALL(*download_item(), GetState())
      .WillByDefault(Return(download::DownloadItem::COMPLETE));
  ON_CALL(*download_item(), CanShowInFolder()).WillByDefault(Return(true));
  info_->SetQuickActionsForTesting(
      {{DownloadCommands::PAUSE, u"label",
        &(features::IsRoundedIconsEnabled() ? vector_icons::kPauseFilledIcon
                                            : vector_icons::kPauseOldIcon)},
       {DownloadCommands::SHOW_IN_FOLDER, u"label",
        &(features::IsRoundedIconsEnabled() ? vector_icons::kFolderFilledIcon
                                            : vector_icons::kFolderOldIcon)}});
  download_item()->NotifyObserversDownloadUpdated();
  ASSERT_EQ(row_view()->info().quick_actions().size(), 2u);

  // Should not be available because they are not present in the ui_info.
  EXPECT_FALSE(row_view()->IsQuickActionButtonVisibleForTesting(
      DownloadCommands::OPEN_WHEN_COMPLETE));
  EXPECT_FALSE(row_view()->IsQuickActionButtonVisibleForTesting(
      DownloadCommands::RESUME));
  EXPECT_FALSE(row_view()->IsQuickActionButtonVisibleForTesting(
      DownloadCommands::CANCEL));
  // Should not be available because the download is complete.
  ASSERT_FALSE(DownloadCommands(row_view()->model()->GetWeakPtr())
                   .IsCommandEnabled(DownloadCommands::PAUSE));
  EXPECT_FALSE(row_view()->IsQuickActionButtonVisibleForTesting(
      DownloadCommands::PAUSE));
  // Should be available because it is present in the ui_info, and the
  // DownloadItem state allows for this command.
  ASSERT_TRUE(DownloadCommands(row_view()->model()->GetWeakPtr())
                  .IsCommandEnabled(DownloadCommands::SHOW_IN_FOLDER));
  EXPECT_TRUE(row_view()->IsQuickActionButtonVisibleForTesting(
      DownloadCommands::SHOW_IN_FOLDER));
}

// Test that the input protector can deny button clicks.
TEST_F(DownloadBubbleRowViewTest, InputProtectorDeniesClicks) {
  EXPECT_CALL(*input_protector_, IsPossiblyUnintendedInteraction(_, _, _))
      .WillRepeatedly(Return(true));

  // Test main button
  EXPECT_CALL(*download_item(), OpenDownload()).Times(0);
  row_view()->SimulateMainButtonClickForTesting(ui::test::TestEvent());

  // Test quick action button.
  ON_CALL(*download_item(), GetState())
      .WillByDefault(Return(download::DownloadItem::COMPLETE));
  ON_CALL(*download_item(), CanOpenDownload()).WillByDefault(Return(true));
  info_->SetQuickActionsForTesting(
      {{DownloadCommands::OPEN_WHEN_COMPLETE, u"label",
        &(features::IsRoundedIconsEnabled() ? vector_icons::kFolderFilledIcon
                                            : vector_icons::kFolderOldIcon)}});
  download_item()->NotifyObserversDownloadUpdated();
  ASSERT_TRUE(row_view()->IsQuickActionButtonVisibleForTesting(
      DownloadCommands::OPEN_WHEN_COMPLETE));

  EXPECT_CALL(*download_item(), OpenDownload()).Times(0);
  ui::MouseEvent event(ui::EventType::kMousePressed, gfx::PointF(),
                       gfx::PointF(), base::TimeTicks::Now(), 0, 0);
  row_view()
      ->GetQuickActionButtonForTesting(DownloadCommands::OPEN_WHEN_COMPLETE)
      ->OnMousePressed(event);
}

// Test that all button controls on the row are disabled while the row is
// occluded by a picture-in-picture window, and re-enabled when it is not.
TEST_F(DownloadBubbleRowViewTest, OcclusionDisablesButtons) {
  views::Button* transparent_button = row_view()->transparent_button();
  views::ImageButton* quick_action =
      row_view()->GetQuickActionButtonForTesting(DownloadCommands::CANCEL);
  views::MdTextButton* main_page_button =
      row_view()->GetMainPageButtonForTesting(DownloadCommands::KEEP);

  ASSERT_TRUE(transparent_button->GetEnabled());
  ASSERT_TRUE(quick_action->GetEnabled());
  ASSERT_TRUE(main_page_button->GetEnabled());

  row_view()->OnOcclusionStateChanged(/*occluded=*/true);
  EXPECT_FALSE(transparent_button->GetEnabled());
  EXPECT_FALSE(quick_action->GetEnabled());
  EXPECT_FALSE(main_page_button->GetEnabled());

  row_view()->OnOcclusionStateChanged(/*occluded=*/false);
  EXPECT_TRUE(transparent_button->GetEnabled());
  EXPECT_TRUE(quick_action->GetEnabled());
  EXPECT_TRUE(main_page_button->GetEnabled());
}

}  // namespace
