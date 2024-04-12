// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/lens/search_bubble_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/webui/lens/search_bubble_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "ui/views/view_class_properties.h"

namespace lens {

constexpr int kBubbleCornerRadius = 20;

class SearchBubbleDialogView : public WebUIBubbleDialogView {
  METADATA_HEADER(SearchBubbleDialogView, WebUIBubbleDialogView)
 public:
  explicit SearchBubbleDialogView(
      views::View* anchor_view,
      std::unique_ptr<WebUIContentsWrapper> contents_wrapper)
      : WebUIBubbleDialogView(anchor_view, contents_wrapper->GetWeakPtr()),
        contents_wrapper_(std::move(contents_wrapper)) {
    // This bubble persists even when deactivated. It must be closed
    // through the SearchBubbleController.
    set_close_on_deactivate(false);
    set_corner_radius(kBubbleCornerRadius);
  }

 private:
  std::unique_ptr<WebUIContentsWrapper> contents_wrapper_;
};

BEGIN_METADATA(SearchBubbleDialogView)
END_METADATA

SearchBubbleController::~SearchBubbleController() = default;

void SearchBubbleController::Show() {
  if (bubble_view_) {
    return;
  }

  auto contents_wrapper =
      std::make_unique<WebUIContentsWrapperT<SearchBubbleUI>>(
          GURL(chrome::kChromeUILensSearchBubbleURL), GetBrowser().profile(),
          IDS_LENS_SEARCH_BUBBLE_DIALOG_TITLE);

  std::unique_ptr<SearchBubbleDialogView> bubble_view =
      std::make_unique<SearchBubbleDialogView>(
          BrowserView::GetBrowserViewForBrowser(&GetBrowser())->toolbar(),
          std::move(contents_wrapper));
  bubble_view->SetProperty(views::kElementIdentifierKey,
                           kLensSearchBubbleElementId);
  bubble_view_ = bubble_view->GetWeakPtr();
  views::BubbleDialogDelegateView::CreateBubble(std::move(bubble_view));
}

void SearchBubbleController::Close() {
  if (!bubble_view_) {
    return;
  }
  DCHECK(bubble_view_->GetWidget());
  bubble_view_->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kUnspecified);
}

SearchBubbleController::SearchBubbleController(Browser* browser)
    : BrowserUserData<SearchBubbleController>(*browser) {}

BROWSER_USER_DATA_KEY_IMPL(SearchBubbleController);

}  // namespace lens
