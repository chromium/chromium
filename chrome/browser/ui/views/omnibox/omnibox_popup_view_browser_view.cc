// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_browser_view.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_web_contents_helper.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/layout_provider.h"

class EmbeddedWebView : public views::WebView {
  METADATA_HEADER(EmbeddedWebView, views::WebView)
 public:
  using ResizeCallback = base::RepeatingCallback<void(const gfx::Size&)>;
  EmbeddedWebView(content::BrowserContext* browser_context,
                  ResizeCallback callback);
  ~EmbeddedWebView() override;
  void AddedToWidget() override;
  void ResizeDueToAutoResize(content::WebContents* source,
                             const gfx::Size& new_size) override;

 private:
  ResizeCallback callback_;
};

BEGIN_METADATA(EmbeddedWebView)
END_METADATA

EmbeddedWebView::EmbeddedWebView(content::BrowserContext* browser_context,
                                 ResizeCallback callback)
    : views::WebView(browser_context), callback_(std::move(callback)) {}

EmbeddedWebView::~EmbeddedWebView() = default;

void EmbeddedWebView::AddedToWidget() {
  views::WebView::AddedToWidget();

  // Native windows ignore parent layer clipping. We must explicitly round the
  // native view holder to prevent square corners from peeking through the
  // frame.
  const float corner_radius =
      views::LayoutProvider::Get()->GetCornerRadiusMetric(
          views::ShapeContextTokens::kOmniboxExpandedRadius);
  gfx::RoundedCornersF rounded_corner_radii = gfx::RoundedCornersF(
      corner_radius, corner_radius, corner_radius, corner_radius);
  holder()->SetCornerRadii(rounded_corner_radii);
}

void EmbeddedWebView::ResizeDueToAutoResize(content::WebContents* source,
                                            const gfx::Size& new_size) {
  views::WebView::ResizeDueToAutoResize(source, new_size);
  if (callback_) {
    callback_.Run(new_size);
  }
}

OmniboxPopupViewBrowserView::OmniboxPopupViewBrowserView(
    LocationBarView* location_bar_view,
    Browser* browser)
    : OmniboxPopupView(location_bar_view->GetOmniboxController()),
      location_bar_view_(location_bar_view),
      browser_(browser) {
  controller()->edit_model()->set_popup_view(this);
  edit_model_observation_.Observe(controller()->edit_model());
}

OmniboxPopupViewBrowserView::~OmniboxPopupViewBrowserView() {
  controller()->edit_model()->set_popup_view(nullptr);
  auto* frame = popup_frame();
  if (frame && frame->parent()) {
    web_view_ = nullptr;
    frame->parent()->RemoveChildViewT(frame);
  }
}

RoundedOmniboxResultsFrame* OmniboxPopupViewBrowserView::popup_frame() {
  return static_cast<RoundedOmniboxResultsFrame*>(popup_frame_tracker_.view());
}

bool OmniboxPopupViewBrowserView::IsOpen() const {
  return web_view_ && web_view_->GetVisible();
}

void OmniboxPopupViewBrowserView::OnContentsChanged() {
  UpdatePopupAppearance();
}

void OmniboxPopupViewBrowserView::InvalidateLine(size_t line) {}

void OmniboxPopupViewBrowserView::UpdatePopupAppearance() {
  const bool has_results =
      !controller()->autocomplete_controller()->result().empty();

  const bool should_be_visible =
      controller()->popup_state_manager()->popup_state() !=
          OmniboxPopupState::kAim &&
      (has_results || (omnibox::IsWebUIOmniboxFullPopupEnabled() &&
                       controller()->edit_model()->has_focus())) &&
      !location_bar_view_->GetOmniboxView()->IsImeShowingPopup();

  if (!should_be_visible) {
    if (popup_frame() && popup_frame()->GetVisible()) {
      popup_frame()->SetVisible(false);
      controller()->popup_state_manager()->SetPopupState(
          OmniboxPopupState::kNone);
    }
  } else {
    if (!popup_frame()) {
      // Defer until initialized in UpdateLayout.
      return;
    }
    popup_frame()->SetVisible(true);

    controller()->popup_state_manager()->SetPopupState(
        OmniboxPopupState::kFull);

    UpdateLayout();
  }
}

void OmniboxPopupViewBrowserView::ProvideButtonFocusHint(size_t line) {}

void OmniboxPopupViewBrowserView::OnDragCanceled() {}

void OmniboxPopupViewBrowserView::GetPopupAccessibleNodeData(
    ui::AXNodeData* node_data) const {}

bool OmniboxPopupViewBrowserView::IsSelectionPopupControlled() const {
  return false;
}

void OmniboxPopupViewBrowserView::UpdateLayout() {
  views::View* omnibox = location_bar_view_;
  gfx::Rect bounds = omnibox->GetLocalBounds();
  if (bounds.width() == 0) {
    return;  // Wait for valid layout
  }

  CHECK(browser_view_);

  if (!web_view_) {
    // Lazily initialize the WebView when the width is known.
    // This is deferred because the location bar width may be 0 during
    // early construction, and passing 0 to EnableSizingFromWebContents
    // triggers a crash.
    auto web_view = std::make_unique<EmbeddedWebView>(
        browser_->profile(),
        base::BindRepeating(&OmniboxPopupViewBrowserView::OnWebViewResize,
                            base::Unretained(this)));
    web_view->LoadInitialURL(GURL("chrome://omnibox-popup.top-chrome/"));
    web_view_ = web_view.get();

    content::WebContents* web_contents = web_view->GetWebContents();
    if (web_contents) {
      OmniboxPopupWebContentsHelper::CreateForWebContents(web_contents);
      auto* helper =
          OmniboxPopupWebContentsHelper::FromWebContents(web_contents);
      if (helper) {
        helper->set_omnibox_controller(controller());
      }
    }

    auto frame = std::make_unique<RoundedOmniboxResultsFrame>(
        web_view.release(), location_bar_view_, true);
    frame->SetCutoutVisibility(false);  // No cutout needed for full popup.
    auto* frame_ptr = browser_view_->AddChildView(std::move(frame));
    popup_frame_tracker_.SetView(frame_ptr);
    frame_ptr->SetVisible(false);

    // Re-evaluate appearance now that we have a view.
    UpdatePopupAppearance();
  }

  if (!web_view_->GetVisible()) {
    return;
  }

  bounds = views::View::ConvertRectToTarget(omnibox, browser_view_, bounds);
  const int initial_height =
      bounds.height() > 0
          ? bounds.height()
          : GetLayoutConstant(LayoutConstant::kLocationBarHeight);

  web_view_->EnableSizingFromWebContents(
      gfx::Size(bounds.width(), initial_height),
      gfx::Size(bounds.width(), INT_MAX));

  // Calculate bounds for the frame.
  gfx::Rect frame_bounds = bounds;
  gfx::Insets alignment_insets =
      RoundedOmniboxResultsFrame::GetLocationBarAlignmentInsets();
  // Only outset vertically to keep exact width.
  frame_bounds.Inset(gfx::Insets::TLBR(-alignment_insets.top(), 0,
                                       -alignment_insets.bottom(), 0));

  // Use the preferred content height if it's larger than the initial location
  // bar height, otherwise enforce the minimum height.
  if (content_height_ > initial_height + alignment_insets.height()) {
    frame_bounds.set_height(content_height_);
  } else {
    frame_bounds.set_height(initial_height + alignment_insets.height());
  }

  // Outset by shadow insets to account for BubbleBorder.
  gfx::Insets shadow_insets = RoundedOmniboxResultsFrame::GetShadowInsets();
  frame_bounds.Inset(
      gfx::Insets::TLBR(-shadow_insets.top(), -shadow_insets.left(),
                        -shadow_insets.bottom(), -shadow_insets.right()));

  popup_frame()->SetBoundsRect(frame_bounds);
}

void OmniboxPopupViewBrowserView::OnWebViewResize(const gfx::Size& new_size) {
  content_height_ = new_size.height();
  UpdateLayout();
}

OmniboxPopupViewBrowserView*
OmniboxPopupViewBrowserView::AsOmniboxPopupViewBrowserView() {
  return this;
}
