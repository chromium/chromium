// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_bubble_row_view.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/download_item_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "ui/events/types/event_type.h"
#include "ui/views/input_event_activation_protector.h"
#include "ui/views/test/mock_input_event_activation_protector.h"

namespace {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::ReturnRefOfCopy;

constexpr int kTimeSinceDownloadCompletedUpdateSeconds = 60;

class DownloadBubbleRowViewTest : public TestWithBrowserView {
 public:
  DownloadBubbleRowViewTest()
      : TestWithBrowserView(
            content::BrowserTaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitAndEnableFeature(safe_browsing::kDownloadBubble);
  }

  DownloadBubbleRowViewTest(const DownloadBubbleRowViewTest&) = delete;
  DownloadBubbleRowViewTest& operator=(const DownloadBubbleRowViewTest&) =
      delete;

  void SetUp() override {
    TestWithBrowserView::SetUp();

    content::DownloadItemUtils::AttachInfoForTesting(
        &download_item_, browser()->profile(), nullptr);
    ON_CALL(download_item_, GetURL())
        .WillByDefault(ReturnRef(GURL::EmptyGURL()));

    DownloadToolbarButtonView* button =
        browser_view()->toolbar()->download_button();
    row_list_view_ = std::make_unique<DownloadBubbleRowListView>(
        /*is_partial_view=*/true, browser());
    const int bubble_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
        views::DISTANCE_BUBBLE_PREFERRED_WIDTH);
    row_view_ = std::make_unique<DownloadBubbleRowView>(
        DownloadItemModel::Wrap(
            &download_item_,
            std::make_unique<DownloadUIModel::BubbleStatusTextBuilder>()),
        row_list_view_.get(), button->bubble_controller(), button, browser(),
        bubble_width);

    auto input_protector =
        std::make_unique<NiceMock<views::MockInputEventActivationProtector>>();
    input_protector_ = input_protector.get();
    ON_CALL(*input_protector_, IsPossiblyUnintendedInteraction(_))
        .WillByDefault(Return(false));
    row_view_->SetInputProtectorForTesting(std::move(input_protector));
  }

  void FastForward(base::TimeDelta time) {
    task_environment()->FastForwardBy(time);
  }

  DownloadBubbleRowView* row_view() { return row_view_.get(); }
  download::MockDownloadItem* download_item() { return &download_item_; }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  NiceMock<download::MockDownloadItem> download_item_;
  std::unique_ptr<DownloadBubbleRowListView> row_list_view_;
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

// Test that the input protector can deny button clicks.
TEST_F(DownloadBubbleRowViewTest, InputProtectorDeniesClicks) {
  EXPECT_CALL(*input_protector_, IsPossiblyUnintendedInteraction(_))
      .WillRepeatedly(Return(true));

  // Test main button
  EXPECT_CALL(*download_item(), OpenDownload()).Times(0);
  row_view()->SimulateMainButtonClickForTesting(ui::test::TestEvent());

  // Test quick action button.
  ON_CALL(*download_item(), GetState())
      .WillByDefault(Return(download::DownloadItem::COMPLETE));
  ON_CALL(*download_item(), CanOpenDownload()).WillByDefault(Return(true));
  download_item()->NotifyObserversDownloadUpdated();
  row_view()->SetUIInfoForTesting(
      DownloadUIModel::BubbleUIInfo().AddQuickAction(
          DownloadCommands::OPEN_WHEN_COMPLETE, u"label",
          &vector_icons::kFolderIcon));
  ASSERT_TRUE(row_view()->IsQuickActionButtonVisibleForTesting(
      DownloadCommands::OPEN_WHEN_COMPLETE));

  EXPECT_CALL(*download_item(), OpenDownload()).Times(0);
  ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::PointF(), gfx::PointF(),
                       base::TimeTicks::Now(), 0, 0);
  row_view()
      ->GetQuickActionButtonForTesting(DownloadCommands::OPEN_WHEN_COMPLETE)
      ->OnMousePressed(event);
}

}  // namespace
