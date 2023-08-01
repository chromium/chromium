// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_bubble_contents_view.h"

#include <utility>

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_partial_view.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_primary_view.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_list_view.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_security_view.h"
#include "chrome/browser/ui/views/download/bubble/download_dialog_view.h"
#include "chrome/browser/ui/views/download/bubble/download_toolbar_button_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"

DownloadBubbleContentsView::DownloadBubbleContentsView(
    base::WeakPtr<Browser> browser,
    base::WeakPtr<DownloadBubbleUIController> bubble_controller,
    base::WeakPtr<DownloadBubbleNavigationHandler> navigation_handler,
    bool primary_view_is_partial_view,
    std::vector<DownloadUIModel::DownloadUIModelPtr> primary_view_models,
    views::BubbleDialogDelegate* bubble_delegate) {
  CHECK(!primary_view_models.empty());
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);

  std::unique_ptr<DownloadBubblePrimaryView> primary_view;
  if (primary_view_is_partial_view) {
    primary_view = std::make_unique<DownloadBubblePartialView>(
        browser, bubble_controller, navigation_handler,
        std::move(primary_view_models),
        base::BindOnce(&DownloadBubbleNavigationHandler::OnDialogInteracted,
                       navigation_handler));
  } else {
    primary_view = std::make_unique<DownloadDialogView>(
        browser, bubble_controller, navigation_handler,
        std::move(primary_view_models));
  }

  primary_view_ = AddChildView(std::move(primary_view));
  security_view_ = AddChildView(std::make_unique<DownloadBubbleSecurityView>(
      bubble_controller, navigation_handler, bubble_delegate));

  // Starts on the primary page.
  SwitchToCurrentPage();
}

DownloadBubbleContentsView::~DownloadBubbleContentsView() {
  security_view_->UpdateSecurityView(nullptr);
}

DownloadBubbleRowView* DownloadBubbleContentsView::GetPrimaryViewRowForTesting(
    size_t index) {
  return primary_view_->GetRowForTesting(index);  // IN-TEST
}

void DownloadBubbleContentsView::ShowPage(Page page) {
  if (page_ == page) {
    return;
  }
  page_ = page;
  CHECK(page != Page::kSecurity || security_view_->IsInitialized());
  SwitchToCurrentPage();
}

DownloadBubbleContentsView::Page DownloadBubbleContentsView::VisiblePage()
    const {
  return page_;
}

void DownloadBubbleContentsView::UpdateSecurityView(
    DownloadBubbleRowView* row) {
  security_view_->UpdateSecurityView(row);
}

void DownloadBubbleContentsView::SwitchToCurrentPage() {
  primary_view_->SetVisible(false);
  security_view_->SetVisible(false);

  switch (page_) {
    case Page::kPrimary:
      primary_view_->SetVisible(true);
      break;
    case Page::kSecurity:
      security_view_->UpdateAccessibilityTextAndFocus();
      security_view_->SetVisible(true);
      break;
  }
}

BEGIN_METADATA(DownloadBubbleContentsView, views::View)
END_METADATA
