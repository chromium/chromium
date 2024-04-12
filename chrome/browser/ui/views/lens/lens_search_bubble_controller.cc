// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/lens/lens_search_bubble_controller.h"

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
constexpr int kBubbleAnchorOffset = -4;

class LensSearchBubbleDialogView : public WebUIBubbleDialogView {
  METADATA_HEADER(LensSearchBubbleDialogView, WebUIBubbleDialogView)
 public:
  explicit LensSearchBubbleDialogView(
      views::View* anchor_view,
      std::unique_ptr<WebUIContentsWrapper> contents_wrapper)
      : WebUIBubbleDialogView(anchor_view, contents_wrapper->GetWeakPtr()),
        contents_wrapper_(std::move(contents_wrapper)) {
    // This bubble persists even when deactivated. It must be closed
    // through the LensSearchBubbleController.
    set_close_on_deactivate(false);
    set_corner_radius(kBubbleCornerRadius);
  }

  gfx::Rect GetAnchorRect() const override {
    auto anchor_rect = BubbleDialogDelegateView::GetAnchorRect();
    anchor_rect.Offset(0, kBubbleAnchorOffset);
    return anchor_rect;
  }

 private:
  std::unique_ptr<WebUIContentsWrapper> contents_wrapper_;
};

BEGIN_METADATA(LensSearchBubbleDialogView)
END_METADATA

LensSearchBubbleController::~LensSearchBubbleController() = default;

void LensSearchBubbleController::Show() {
  if (bubble_view_) {
    return;
  }

  auto contents_wrapper =
      std::make_unique<WebUIContentsWrapperT<SearchBubbleUI>>(
          GURL(chrome::kChromeUILensSearchBubbleURL), GetBrowser().profile(),
          IDS_LENS_SEARCH_BUBBLE_DIALOG_TITLE,
          /*webui_resizes_host=*/true,
          /*esc_closes_ui=*/true,
          /*supports_draggable_regions=*/false);

  std::unique_ptr<LensSearchBubbleDialogView> bubble_view =
      std::make_unique<LensSearchBubbleDialogView>(
          BrowserView::GetBrowserViewForBrowser(&GetBrowser())->top_container(),
          std::move(contents_wrapper));
  bubble_view->SetProperty(views::kElementIdentifierKey,
                           kLensSearchBubbleElementId);
  bubble_view_ = bubble_view->GetWeakPtr();
  views::BubbleDialogDelegateView::CreateBubble(std::move(bubble_view));
}

void LensSearchBubbleController::Close() {
  if (!bubble_view_) {
    return;
  }
  DCHECK(bubble_view_->GetWidget());
  bubble_view_->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kUnspecified);
}

LensSearchBubbleController::LensSearchBubbleController(Browser* browser)
    : BrowserUserData<LensSearchBubbleController>(*browser) {}

BROWSER_USER_DATA_KEY_IMPL(LensSearchBubbleController);

}  // namespace lens
