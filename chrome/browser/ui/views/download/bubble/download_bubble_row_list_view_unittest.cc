// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_bubble_row_list_view.h"

#include <optional>

#include "base/strings/string_number_conversions.h"
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
#include "content/public/browser/download_item_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/views_test_utils.h"

namespace {

using ::offline_items_collection::ContentId;
using ::testing::NiceMock;
using ::testing::ReturnRefOfCopy;

class DownloadBubbleRowListViewTest : public TestWithBrowserView {
 public:
  DownloadBubbleRowListViewTest() = default;
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

  // Sets up `row_list_view_` with the first `initial_item_count` of
  // `download_items_`.
  void InitRowListView(int initial_item_count) {
    std::vector<DownloadUIModel::DownloadUIModelPtr> models;
    for (int i = 0; i < initial_item_count; ++i) {
      models.push_back(MakeModel(i));
    }
    info_ = std::make_unique<DownloadBubbleRowListViewInfo>(std::move(models));
    const int bubble_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
        views::DISTANCE_BUBBLE_PREFERRED_WIDTH);
    row_list_view_ = std::make_unique<DownloadBubbleRowListView>(
        browser()->AsWeakPtr(),
        toolbar_button()->bubble_controller()->GetWeakPtr(),
        toolbar_button()->GetWeakPtr(), bubble_width, *info_);
  }

  // Creates a `DownloadUIModel` for the download item at `index` in
  // `download_items_`.
  DownloadUIModel::DownloadUIModelPtr MakeModel(int index) {
    return DownloadItemModel::Wrap(
        download_items_[index].get(),
        std::make_unique<DownloadUIModel::BubbleStatusTextBuilder>());
  }

  ContentId GetIdForItem(int index) {
    return OfflineItemUtils::GetContentIdForDownload(
        download_items_[index].get());
  }

 protected:
  std::unique_ptr<DownloadBubbleRowListViewInfo> info_;
  std::unique_ptr<DownloadBubbleRowListView> row_list_view_;
  std::vector<std::unique_ptr<NiceMock<download::MockDownloadItem>>>
      download_items_;
};

TEST_F(DownloadBubbleRowListViewTest, AddRow) {
  InitItems(2);
  InitRowListView(1);
  EXPECT_EQ(row_list_view_->NumRows(), 1u);
  info_->AddRow(MakeModel(1));
  EXPECT_EQ(row_list_view_->NumRows(), 2u);
}

TEST_F(DownloadBubbleRowListViewTest, RemoveRow) {
  InitItems(1);
  InitRowListView(1);
  EXPECT_EQ(row_list_view_->NumRows(), 1u);
  info_->RemoveRow(GetIdForItem(0));
  EXPECT_EQ(row_list_view_->NumRows(), 0u);
}

}  // namespace
