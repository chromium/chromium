// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/contents_web_view.h"

#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/views/status_bubble_views.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/theme_provider.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/views/background.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/wm/core/window_util.h"
#endif

ContentsWebView::ContentsWebView(content::BrowserContext* browser_context)
    : views::WebView(browser_context),
      status_bubble_(nullptr) {
}

ContentsWebView::~ContentsWebView() {
}

void ContentsWebView::SetStatusBubble(StatusBubbleViews* status_bubble) {
  status_bubble_ = status_bubble;
  DCHECK(!status_bubble_ || status_bubble_->base_view() == this);
  if (status_bubble_)
    status_bubble_->Reposition();
}

bool ContentsWebView::GetNeedsNotificationWhenVisibleBoundsChange() const {
  return true;
}

void ContentsWebView::OnVisibleBoundsChanged() {
  if (status_bubble_)
    status_bubble_->Reposition();
}

void ContentsWebView::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  WebView::ViewHierarchyChanged(details);
  if (details.is_add)
    UpdateBackgroundColor();
}

void ContentsWebView::OnThemeChanged() {
  UpdateBackgroundColor();
}

void ContentsWebView::OnLetterboxingChanged() {
  UpdateBackgroundColor();
}

void ContentsWebView::UpdateBackgroundColor() {
  const ui::ThemeProvider* const theme = GetThemeProvider();
  if (!theme)
    return;

  const SkColor ntp_background = color_utils::GetResultingPaintColor(
      theme->GetColor(ThemeProperties::COLOR_NTP_BACKGROUND), SK_ColorWHITE);
  if (is_letterboxing()) {
    // Set the background color to a dark tint of the new tab page's background
    // color.  This is the color filled within the WebView's bounds when its
    // child view is sized specially for fullscreen tab capture.  See WebView
    // header file comments for more details.
    const int kBackgroundBrightness = 0x33;  // 20%
    // Make sure the background is opaque.
    const SkColor dimmed_ntp_background = SkColorSetARGB(
        SkColorGetA(ntp_background),
        SkColorGetR(ntp_background) * kBackgroundBrightness / 0xFF,
        SkColorGetG(ntp_background) * kBackgroundBrightness / 0xFF,
        SkColorGetB(ntp_background) * kBackgroundBrightness / 0xFF);
    SetBackground(views::CreateSolidBackground(dimmed_ntp_background));
  } else {
    SetBackground(views::CreateSolidBackground(ntp_background));
  }
  // Changing a view's background does not necessarily schedule the view to be
  // redrawn.
  SchedulePaint();

  if (web_contents()) {
    content::RenderWidgetHostView* rwhv =
        web_contents()->GetRenderWidgetHostView();
    if (rwhv)
      rwhv->SetBackgroundColor(ntp_background);
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
  if (!web_contents())
    return;
#if defined(USE_AURA)
  // We don't need to clone the layers on non-Aura (Mac), because closing an
  // NSWindow does not animate.
  cloned_layer_tree_ = wm::RecreateLayers(web_contents()->GetNativeView());
#endif
  if (!cloned_layer_tree_ || !cloned_layer_tree_->root()) {
    cloned_layer_tree_.reset();
    return;
  }

  SetPaintToLayer();

  // The cloned layer is in a different coordinate system them our layer (which
  // is now the new parent of the cloned layer). Convert coordinates so that the
  // cloned layer appears at the right location.
  gfx::PointF origin;
  ui::Layer::ConvertPointToLayer(cloned_layer_tree_->root(), layer(), &origin);
  cloned_layer_tree_->root()->SetBounds(
      gfx::Rect(gfx::ToFlooredPoint(origin),
                cloned_layer_tree_->root()->bounds().size()));
  layer()->Add(cloned_layer_tree_->root());
}

void ContentsWebView::DestroyClonedLayer() {
  cloned_layer_tree_.reset();
  DestroyLayer();
}

void ContentsWebView::RenderViewReady() {
  // Set the background color to be the theme's ntp background on startup.
  UpdateBackgroundColor();
  WebView::RenderViewReady();
}
