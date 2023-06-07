// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_bubble_row_view.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/download/bubble/download_bubble_ui_controller.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/download_item_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "ui/events/test/test_event.h"

namespace {

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRefOfCopy;

constexpr int kTimeSinceDownloadCompletedUpdateSeconds = 60;

class DownloadBubbleRowViewTest : public TestWithBrowserView {
 public:
  DownloadBubbleRowViewTest()
      : TestWithBrowserView(
            content::BrowserTaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitWithFeatures(
        {safe_browsing::kDownloadBubble, safe_browsing::kDownloadBubbleV2}, {});
  }

  DownloadBubbleRowViewTest(const DownloadBubbleRowViewTest&) = delete;
  DownloadBubbleRowViewTest& operator=(const DownloadBubbleRowViewTest&) =
      delete;

  void SetUp() override {
    TestWithBrowserView::SetUp();

    content::DownloadItemUtils::AttachInfoForTesting(
        &download_item_, browser()->profile(), nullptr);

    DownloadToolbarButtonView* button =
        browser_view()->toolbar()->download_button();
    row_list_view_ = std::make_unique<DownloadBubbleRowListView>();
    const int bubble_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
        views::DISTANCE_BUBBLE_PREFERRED_WIDTH);
    row_view_ = std::make_unique<DownloadBubbleRowView>(
        DownloadItemModel::Wrap(
            &download_item_,
            std::make_unique<DownloadUIModel::BubbleStatusTextBuilder>()),
        row_list_view_.get(), button->bubble_controller()->GetWeakPtr(),
        button->GetWeakPtr(), browser()->AsWeakPtr(), bubble_width);
  }

  void FastForward(base::TimeDelta time) {
    task_environment()->FastForwardBy(time);
  }

  DownloadBubbleRowView* row_view() { return row_view_.get(); }
  download::MockDownloadItem* download_item() { return &download_item_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  NiceMock<download::MockDownloadItem> download_item_;
  std::unique_ptr<DownloadBubbleRowListView> row_list_view_;
  std::unique_ptr<DownloadBubbleRowView> row_view_;
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

  ui::Accelerator accelerator;
  ASSERT_TRUE(browser_view()->GetAccelerator(IDC_COPY, &accelerator));

  row_view()->AcceleratorPressed(accelerator);

  std::vector<ui::FileInfo> filenames;
  clipboard->ReadFilenames(ui::ClipboardBuffer::kCopyPaste, nullptr,
                           &filenames);
  ASSERT_EQ(filenames.size(), 1u);
  EXPECT_EQ(filenames[0].path, target_path);

  clipboard->DestroyClipboardForCurrentThread();
}

TEST_F(DownloadBubbleRowViewTest, UpdateTimeFromCompletedDownload) {
  ON_CALL(*download_item(), GetState())
      .WillByDefault(Return(download::DownloadItem::COMPLETE));
  ON_CALL(*download_item(), GetEndTime())
      .WillByDefault(Return(base::Time::Now()));
  row_view()->OnDownloadUpdated();
  // Get starting label for a finished download and ensure it stays
  // the same until one timer interval.
  std::u16string row_label = row_view()->GetSecondaryLabelTextForTesting();
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
  download_item()->NotifyObserversDownloadUpdated();
  row_view()->SetUIInfoForTesting(
      DownloadUIModel::BubbleUIInfo()
          .AddQuickAction(DownloadCommands::PAUSE, u"label",
                          &vector_icons::kPauseIcon)
          .AddQuickAction(DownloadCommands::SHOW_IN_FOLDER, u"label",
                          &vector_icons::kFolderIcon));
  ASSERT_EQ(row_view()->ui_info().quick_actions.size(), 2u);

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

}  // namespace
