// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_bubble_row_view.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/download_item_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/test/test_clipboard.h"

namespace {

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRefOfCopy;

class DownloadBubbleRowViewTest : public TestWithBrowserView {
 public:
  DownloadBubbleRowViewTest() {
    scoped_feature_list_.InitAndEnableFeature(safe_browsing::kDownloadBubble);
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
    row_list_view_ = std::make_unique<DownloadBubbleRowListView>(
        /*is_partial_view=*/true, browser());
    row_view_ = std::make_unique<DownloadBubbleRowView>(
        DownloadItemModel::Wrap(&download_item_), row_list_view_.get(),
        button->bubble_controller(), button, browser());
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

}  // namespace
