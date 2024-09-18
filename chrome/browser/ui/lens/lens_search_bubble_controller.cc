// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_search_bubble_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_controller_glue.h"
#include "chrome/browser/ui/lens/search_bubble_ui.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view_class_properties.h"

namespace lens {

constexpr int kBubbleCornerRadius = 20;
constexpr int kBubbleAnchorOffset = -4;

class LensSearchBubbleDialogView : public WebUIBubbleDialogView {
  METADATA_HEADER(LensSearchBubbleDialogView, WebUIBubbleDialogView)
 public:
  explicit LensSearchBubbleDialogView(
      views::View* anchor_view,
      std::unique_ptr<WebUIContentsWrapper> contents_wrapper,
      base::WeakPtr<LensSearchBubbleController> search_bubble_controller)
      : WebUIBubbleDialogView(anchor_view, contents_wrapper->GetWeakPtr()),
        contents_wrapper_(std::move(contents_wrapper)),
        search_bubble_controller_(search_bubble_controller) {
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

  // WebUIContentsWrapper::Host:
  void CloseUI() override {
    WebUIBubbleDialogView::CloseUI();
    search_bubble_controller_->RemoveLensOverlayControllerGlue();
    // The lens overlay controller's CloseUIAsync() will eventually call the
    // search bubble's Close() function.
    search_bubble_controller_->CloseLensOverlay();
  }

 private:
  std::unique_ptr<WebUIContentsWrapper> contents_wrapper_;
  base::WeakPtr<LensSearchBubbleController> search_bubble_controller_;
};

BEGIN_METADATA(LensSearchBubbleDialogView)
END_METADATA

LensSearchBubbleController::LensSearchBubbleController(
    LensOverlayController* lens_overlay_controller)
    : lens_overlay_controller_(lens_overlay_controller) {}

LensSearchBubbleController::~LensSearchBubbleController() = default;

void LensSearchBubbleController::Show() {
  if (bubble_view_) {
    return;
  }

  content::WebContents* overlay_web_contents =
      lens_overlay_controller_->GetTabInterface()->GetContents();

  auto contents_wrapper =
      std::make_unique<WebUIContentsWrapperT<SearchBubbleUI>>(
          GURL(chrome::kChromeUILensSearchBubbleURL),
          Profile::FromBrowserContext(
              overlay_web_contents->GetBrowserContext()),
          IDS_LENS_SEARCH_BUBBLE_DIALOG_TITLE,
          /*esc_closes_ui=*/true,
          /*supports_draggable_regions=*/false);
  web_contents_ = contents_wrapper->web_contents();
  lens::LensOverlayControllerGlue::CreateForWebContents(
      web_contents_, lens_overlay_controller_);
  std::unique_ptr<LensSearchBubbleDialogView> bubble_view =
      std::make_unique<LensSearchBubbleDialogView>(
          lens_overlay_controller_->GetTabInterface()
              ->GetBrowserWindowInterface()
              ->TopContainer(),
          std::move(contents_wrapper), weak_factory_.GetWeakPtr());
  bubble_view->SetProperty(views::kElementIdentifierKey,
                           kLensSearchBubbleElementId);
  bubble_view_ = bubble_view->GetWeakPtr();
  views::BubbleDialogDelegateView::CreateBubble(std::move(bubble_view));
}

void LensSearchBubbleController::Close() {
  // Bubble view will still exist if being closed through the lens overlay
  // controller.
  if (bubble_view_) {
    DCHECK(bubble_view_->GetWidget());
    bubble_view_->GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kUnspecified);
    RemoveLensOverlayControllerGlue();
  }
  // RealboxOmniboxClient has a reference to web_contents_ so must reset before
  // web_contents_ gets destroyed.
  contextual_searchbox_handler_.reset();
  bubble_view_ = nullptr;
  web_contents_ = nullptr;
}

bool LensSearchBubbleController::IsSearchBubbleVisible() {
  return bubble_view_ && bubble_view_->GetWidget() &&
         bubble_view_->GetWidget()->IsVisible();
}

void LensSearchBubbleController::CloseLensOverlay() {
  lens_overlay_controller_->CloseUISync(
      lens::LensOverlayDismissalSource::kSearchBubbleCloseButton);
}

void LensSearchBubbleController::RemoveLensOverlayControllerGlue() {
  CHECK(web_contents_);
  web_contents_->RemoveUserData(LensOverlayControllerGlue::UserDataKey());
}

void LensSearchBubbleController::SetContextualSearchboxHandler(
    std::unique_ptr<RealboxHandler> handler) {
  contextual_searchbox_handler_ = std::move(handler);
}

}  // namespace lens
