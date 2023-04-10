// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_bubble_partial_view.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_list_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/flex_layout.h"

// static
std::unique_ptr<DownloadBubblePartialView> DownloadBubblePartialView::Create(
    raw_ptr<Browser> browser,
    raw_ptr<DownloadBubbleUIController> bubble_controller,
    raw_ptr<DownloadBubbleNavigationHandler> navigation_handler,
    std::vector<DownloadUIModel::DownloadUIModelPtr> rows,
    base::OnceClosure on_mouse_entered_closure) {
  if (rows.empty()) {
    return nullptr;
  }

  return base::WrapUnique(new DownloadBubblePartialView(
      browser, bubble_controller, navigation_handler, std::move(rows),
      std::move(on_mouse_entered_closure)));
}

DownloadBubblePartialView::DownloadBubblePartialView(
    raw_ptr<Browser> browser,
    raw_ptr<DownloadBubbleUIController> bubble_controller,
    raw_ptr<DownloadBubbleNavigationHandler> navigation_handler,
    std::vector<DownloadUIModel::DownloadUIModelPtr> rows,
    base::OnceClosure on_mouse_entered_closure)
    : on_mouse_entered_closure_(std::move(on_mouse_entered_closure)) {
  SetNotifyEnterExitOnChild(true);
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);
  const int preferred_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH);

  AddChildView(DownloadBubbleRowListView::CreateWithScroll(
      /*is_partial_view=*/true, browser, bubble_controller, navigation_handler,
      std::move(rows), preferred_width));
}

DownloadBubblePartialView::~DownloadBubblePartialView() = default;

void DownloadBubblePartialView::OnMouseEntered(const ui::MouseEvent& event) {
  if (on_mouse_entered_closure_) {
    std::move(on_mouse_entered_closure_).Run();
  }
}

BEGIN_METADATA(DownloadBubblePartialView, views::View)
END_METADATA
