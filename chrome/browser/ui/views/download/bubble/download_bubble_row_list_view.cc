// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_bubble_row_list_view.h"

#include "chrome/browser/download/bubble/download_bubble_ui_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/scroll_view.h"

namespace {

// 7.5 rows * 60 px per row = 450;
constexpr int kMaxHeightForRowList = 450;

}  // namespace

//  static
std::unique_ptr<views::ScrollView> DownloadBubbleRowListView::CreateWithScroll(
    base::WeakPtr<Browser> browser,
    base::WeakPtr<DownloadBubbleUIController> bubble_controller,
    base::WeakPtr<DownloadBubbleNavigationHandler> navigation_handler,
    std::vector<DownloadUIModel::DownloadUIModelPtr> rows,
    int fixed_width) {
  auto row_list_view = std::make_unique<DownloadBubbleRowListView>();
  for (DownloadUIModel::DownloadUIModelPtr& model : rows) {
    // raw pointer for `row_list_view` is safe as the toolbar owns the bubble,
    // which owns an individual row view. Note we need to copy rather than move
    // the WeakPtrs so that each row view gets a valid pointer.
    row_list_view->AddChildView(std::make_unique<DownloadBubbleRowView>(
        std::move(model), row_list_view.get(), bubble_controller,
        navigation_handler, browser, fixed_width));
  }

  auto scroll_view = std::make_unique<views::ScrollView>();
  scroll_view->SetContents(std::move(row_list_view));
  scroll_view->ClipHeightTo(0, kMaxHeightForRowList);
  scroll_view->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  scroll_view->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kEnabled);
  return scroll_view;
}

DownloadBubbleRowListView::DownloadBubbleRowListView() {
  SetOrientation(views::LayoutOrientation::kVertical);
  SetNotifyEnterExitOnChild(true);
}

DownloadBubbleRowListView::~DownloadBubbleRowListView() = default;

BEGIN_METADATA(DownloadBubbleRowListView, views::FlexLayoutView)
END_METADATA
