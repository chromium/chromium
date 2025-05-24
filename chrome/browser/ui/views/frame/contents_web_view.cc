// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/contents_web_view.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/frame/web_contents_close_handler.h"
#include "chrome/browser/ui/views/status_bubble_views.h"
#include "components/search/ntp_features.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/layer_type.h"
#include "ui/views/view_class_properties.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/wm/core/window_util.h"
#endif

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ContentsWebView,
                                      kContentsWebViewElementId);

ContentsWebView::ContentsWebView(content::BrowserContext* browser_context)
    : views::WebView(browser_context) {
  // Draws the ContentsWebView background.
  SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  SetProperty(views::kElementIdentifierKey, kContentsWebViewElementId);
  status_bubble_ = std::make_unique<StatusBubbleViews>(this);
  status_bubble_->Reposition();
  web_contents_close_handler_ = std::make_unique<WebContentsCloseHandler>(this);
}

ContentsWebView::~ContentsWebView() = default;

StatusBubbleViews* ContentsWebView::GetStatusBubble() const {
  if (status_bubble_) {
    return status_bubble_.get();
  }
  return nullptr;
}

WebContentsCloseHandler* ContentsWebView::GetWebContentsCloseHandler() const {
  return web_contents_close_handler_.get();
}

void ContentsWebView::SetBackgroundVisible(bool background_visible) {
  background_visible_ = background_visible;
  if (GetWidget()) {
    UpdateBackgroundColor();
  }
}

const gfx::RoundedCornersF& ContentsWebView::GetBackgroundRadii() const {
  const ui::Layer* background_layer = layer();

  CHECK(background_layer);
  return background_layer->rounded_corner_radii();
}

void ContentsWebView::SetBackgroundRadii(const gfx::RoundedCornersF& radii) {
  ui::Layer* background_layer = layer();

  CHECK(background_layer);
  background_layer->SetRoundedCornerRadius(radii);
  background_layer->SetIsFastRoundedCorner(true);
}

bool ContentsWebView::GetNeedsNotificationWhenVisibleBoundsChange() const {
  return true;
}

void ContentsWebView::OnVisibleBoundsChanged() {
  if (status_bubble_) {
    status_bubble_->Reposition();
  }
}

void ContentsWebView::OnThemeChanged() {
  views::WebView::OnThemeChanged();
  UpdateBackgroundColor();
}

void ContentsWebView::OnLetterboxingChanged() {
  if (GetWidget()) {
    UpdateBackgroundColor();
  }
}

void ContentsWebView::SetWebContents(content::WebContents* web_contents) {
  views::WebView::SetWebContents(web_contents);
  if (web_contents == nullptr) {
    status_bubble_ = nullptr;
    // Early exit: Without web contents, views dependent on ContentsWebView's
    // bounds cannot be properly created or positioned. These views will
    // initialize later when valid web contents exist.
    return;
  }

  if (status_bubble_ == nullptr) {
    status_bubble_ = std::make_unique<StatusBubbleViews>(this);
    status_bubble_->Reposition();
  }
}

void ContentsWebView::UpdateBackgroundColor() {
  const SkColor color = GetColorProvider()->GetColor(
      is_letterboxing() ? kColorWebContentsBackgroundLetterboxing
                        : kColorWebContentsBackground);

  ui::Layer* background_layer = layer();
  background_layer->SetColor(background_visible_ ? color : SK_ColorTRANSPARENT);

  if (web_contents()) {
    content::RenderWidgetHostView* rwhv =
        web_contents()->GetRenderWidgetHostView();
    if (rwhv) {
      rwhv->SetBackgroundColor(background_visible_ ? color
                                                   : SK_ColorTRANSPARENT);
    }
  }
}

std::unique_ptr<ui::Layer> ContentsWebView::RecreateLayer() {
  std::unique_ptr<ui::Layer> old_layer = View::RecreateLayer();

  if (cloned_layer_tree_ && old_layer) {
    // Our layer has been recreated and we have a clone of the WebContents
    // layer. Combined this means we're about to be destroyed and an animation
    // is in effect. The animation cloned our layer, but it won't create another
    // clone of the WebContents layer (|cloned_layer_tree_|). Another clone
    // is not created as the clone has no owner (see CloneChildren()). Because
    // we want the WebContents layer clone to be animated we move it to the
    // old_layer, which is the layer the animation happens on. This animation
    // ends up owning the layer (and all its descendants).
    old_layer->Add(cloned_layer_tree_->release());
    cloned_layer_tree_.reset();
  }

  return old_layer;
}

void ContentsWebView::CloneWebContentsLayer() {
  if (!web_contents()) {
    return;
  }
#if defined(USE_AURA)
  // We don't need to clone the layers on non-Aura (Mac), because closing an
  // NSWindow does not animate.
  cloned_layer_tree_ = wm::RecreateLayers(web_contents()->GetNativeView());
#endif
  if (!cloned_layer_tree_ || !cloned_layer_tree_->root()) {
    cloned_layer_tree_.reset();
    return;
  }

  // The cloned layer is in a different coordinate system than our layer (which
  // is now the new parent of the cloned layer). Convert coordinates so that the
  // cloned layer appears at the right location.
  gfx::PointF origin;
  ui::Layer::ConvertPointToLayer(cloned_layer_tree_->root(), layer(),
                                 /*use_target_transform=*/true, &origin);
  cloned_layer_tree_->root()->SetBounds(
      gfx::Rect(gfx::ToFlooredPoint(origin),
                cloned_layer_tree_->root()->bounds().size()));
  layer()->Add(cloned_layer_tree_->root());
}

void ContentsWebView::DestroyClonedLayer() {
  cloned_layer_tree_.reset();
}

void ContentsWebView::RenderViewReady() {
  // Set the background color to be the theme's ntp background on startup.
  if (GetWidget()) {
    UpdateBackgroundColor();
  }
  WebView::RenderViewReady();
}

BEGIN_METADATA(ContentsWebView)
END_METADATA
