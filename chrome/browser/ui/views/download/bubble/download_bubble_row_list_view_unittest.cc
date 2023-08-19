// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_bubble_row_list_view.h"

#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/download/bubble/download_bubble_ui_controller.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/offline_item_utils.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_view.h"
#include "chrome/browser/ui/views/download/bubble/download_toolbar_button_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/download_item_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/views/test/views_test_utils.h"

namespace {

using ::offline_items_collection::ContentId;
using ::testing::NiceMock;
using ::testing::ReturnRefOfCopy;

class DownloadBubbleRowListViewTest : public TestWithBrowserView {
 public:
  DownloadBubbleRowListViewTest() {
    scoped_feature_list_.InitWithFeatures(
        {safe_browsing::kDownloadBubble, safe_browsing::kDownloadBubbleV2}, {});
  }
  ~DownloadBubbleRowListViewTest() override = default;

  DownloadBubbleRowListViewTest(const DownloadBubbleRowListViewTest&) = delete;
  DownloadBubbleRowListViewTest& operator=(
      const DownloadBubbleRowListViewTest&) = delete;

  void TearDown() override {
    download_items_.clear();
    TestWithBrowserView::TearDown();
  }

  DownloadToolbarButtonView* toolbar_button() {
    return browser_view()->toolbar()->download_button();
  }

  // Sets up `num_items` mock download items with GUID equal to their index in
  // `download_items_`.
  void InitItems(int num_items) {
    for (int i = 0; i < num_items; ++i) {
      auto item = std::make_unique<NiceMock<download::MockDownloadItem>>();
      EXPECT_CALL(*item, GetGuid())
          .WillRepeatedly(ReturnRefOfCopy(base::NumberToString(i)));
      content::DownloadItemUtils::AttachInfoForTesting(
          item.get(), browser()->profile(), nullptr);
      download_items_.push_back(std::move(item));
    }
  }

  // Creates a row view for the download item at `index` in `download_items_`.
  // Pass an optional height to change the resulting view's height.
  std::unique_ptr<DownloadBubbleRowView> MakeRow(int index) {
    const int bubble_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
        views::DISTANCE_BUBBLE_PREFERRED_WIDTH);
    auto row = std::make_unique<DownloadBubbleRowView>(
        DownloadItemModel::Wrap(
            download_items_[index].get(),
            std::make_unique<DownloadUIModel::BubbleStatusTextBuilder>()),
        &row_list_view_, toolbar_button()->bubble_controller()->GetWeakPtr(),
        toolbar_button()->GetWeakPtr(), browser()->AsWeakPtr(), bubble_width);
    return row;
  }

  ContentId GetIdForItem(int index) {
    return OfflineItemUtils::GetContentIdForDownload(
        download_items_[index].get());
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  DownloadBubbleRowListView row_list_view_;
  std::vector<std::unique_ptr<NiceMock<download::MockDownloadItem>>>
      download_items_;
};

TEST_F(DownloadBubbleRowListViewTest, AddRow) {
  EXPECT_EQ(row_list_view_.NumRows(), 0u);
  InitItems(2);
  row_list_view_.AddRow(MakeRow(0));
  EXPECT_EQ(row_list_view_.NumRows(), 1u);
  row_list_view_.AddRow(MakeRow(1));
  EXPECT_EQ(row_list_view_.NumRows(), 2u);
}

TEST_F(DownloadBubbleRowListViewTest, RemoveRow) {
  InitItems(1);
  std::unique_ptr<DownloadBubbleRowView> row = MakeRow(0);
  auto* row_raw = row.get();
  row_list_view_.AddRow(std::move(row));
  EXPECT_EQ(row_list_view_.NumRows(), 1u);
  std::unique_ptr<DownloadBubbleRowView> row_unique =
      row_list_view_.RemoveRow(row_raw);
  EXPECT_EQ(row_list_view_.NumRows(), 0u);
  EXPECT_EQ(row_unique.get(), row_raw);
}

}  // namespace
