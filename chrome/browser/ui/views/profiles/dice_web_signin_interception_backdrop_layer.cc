// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/dice_web_signin_interception_backdrop_layer.h"

#include "base/check.h"
#include "build/build_config.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/profiles/profile_menu_view.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/layer_type.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr SkColor kColorSemitransparentBackdrop =
    SkColorSetARGB(0x9F, 0x00, 0x00, 0x00);

}  // namespace

DiceWebSigninInterceptionBackdropLayer::DiceWebSigninInterceptionBackdropLayer(
    Browser& browser,
    views::View& highlighted_button)
    : backdrop_layer_(ui::LayerType::LAYER_TEXTURED),
      browser_(browser.AsWeakPtr()),
      highlighted_button_(&highlighted_button) {
  // Configure the backdrop layer.
  backdrop_layer_.SetName("BackdropLayer");
  backdrop_layer_.SetFillsBoundsOpaquely(false);
  backdrop_layer_.set_delegate(this);
  backdrop_layer_.SetVisible(true);
  UpdateLayerBounds();

  // Add the backdrop layer on top of the browser.
  BrowserView* browser_view =
      BrowserView::GetBrowserViewForBrowser(browser_.get());
  browser_layer_ = browser_view->GetWidget()->GetLayer();
  browser_layer_->Add(&backdrop_layer_);
  browser_layer_->StackAtTop(&backdrop_layer_);

  highlighted_button_observation_.Observe(highlighted_button_);
  browser_layer_observation_.Observe(browser_layer_);
  browser_frame_observation_.Observe(browser_view->frame());

  // Draw the backdrop layer.
  SchedulePaint();
}

DiceWebSigninInterceptionBackdropLayer::
    ~DiceWebSigninInterceptionBackdropLayer() {
  if (browser_layer_) {
    browser_layer_->Remove(&backdrop_layer_);
  }
  backdrop_layer_.set_delegate(nullptr);
}

void DiceWebSigninInterceptionBackdropLayer::UpdateLayerBounds() {
  // Compute the backdrop bounds, by moving the client area bounds to (0, 0).
  gfx::Rect bounds = BrowserView::GetBrowserViewForBrowser(browser_.get())
                         ->GetWidget()
                         ->GetClientAreaBoundsInScreen();
  bounds.Offset(-bounds.x(), -bounds.y());
  backdrop_layer_.SetBounds(bounds);
}

void DiceWebSigninInterceptionBackdropLayer::SchedulePaint() {
  backdrop_layer_.SchedulePaint(backdrop_layer_.bounds());
}

void DiceWebSigninInterceptionBackdropLayer::DrawDarkBackdrop(
    gfx::Canvas* canvas) {
  BrowserView* browser_view =
      BrowserView::GetBrowserViewForBrowser(browser_.get());
  gfx::Rect dark_backdrop_bounds =
      browser_view->frame()->GetFrameView()->GetBoundsForClientView();

  if (!features::IsChromeRefresh2023()) {
    // On Linux and Windows, before the 2023 refresh, the window frame has a
    // small 4px region at the top, that is reserved as a drag and drop target.
    // The dark layer should be drawn on top of this region, but it's not easily
    // accessible in code, and requires specific implementations.
    // After the 2023 refresh, this drag area no longer exists.
#if BUILDFLAG(IS_LINUX)
    // On linux, the drag area is accessible through the
    // `MirroredFrameBorderInsets()` function, which crashes on non-Linux
    // platforms.
    dark_backdrop_bounds = backdrop_layer_.bounds();
    dark_backdrop_bounds.Inset(
        browser_view->frame()->GetFrameView()->MirroredFrameBorderInsets());
#elif BUILDFLAG(IS_WIN)
    // On Windows, the layer cannot overextend the browser window. Simply draw
    // over the whole area.
    dark_backdrop_bounds = backdrop_layer_.bounds();
#endif
  }

  if (backdrop_layer_.size() == dark_backdrop_bounds.size()) {
    // The layer size matches the window size. The dark color can be applied
    // to the whole area.
    canvas->DrawColor(kColorSemitransparentBackdrop);
  } else {
    // The visible window frame is smaller than the layer, so the layer is
    // first cleared with transparent color and a dark backdrop is then drawn
    // over the window.
    canvas->DrawColor(SK_ColorTRANSPARENT);
    canvas->FillRect(dark_backdrop_bounds, kColorSemitransparentBackdrop);
  }
}

void DiceWebSigninInterceptionBackdropLayer::DrawHighlightedButton(
    gfx::Canvas* canvas) {
  if (!highlighted_button_->IsDrawn()) {
    // The button may not be drawn. For example, the whole toolbar is hidden in
    // full-screen mode.
    return;
  }

  // Draw button in transparent color.
  gfx::Rect bounds = highlighted_button_->GetBoundsInScreen();
  bounds.Offset(-BrowserView::GetBrowserViewForBrowser(browser_.get())
                     ->GetWidget()
                     ->GetClientAreaBoundsInScreen()
                     .OffsetFromOrigin());
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setBlendMode(SkBlendMode::kClear);
  flags.setColor(SK_ColorTRANSPARENT);
  // This assumes that the button has the shape of a "pill" or a circle.
  canvas->DrawRoundRect(bounds, /*radius=*/bounds.height(), flags);
}

void DiceWebSigninInterceptionBackdropLayer::OnPaintLayer(
    const ui::PaintContext& context) {
  if (!browser_ || !highlighted_button_ || !browser_layer_) {
    return;
  }

  ui::PaintRecorder recorder(context, backdrop_layer_.bounds().size());
  gfx::Canvas* canvas = recorder.canvas();
  DrawDarkBackdrop(canvas);
  DrawHighlightedButton(canvas);
}

void DiceWebSigninInterceptionBackdropLayer::LayerDestroyed(ui::Layer* layer) {
  CHECK_EQ(layer, browser_layer_);
  browser_layer_ = nullptr;
  browser_layer_observation_.Reset();
}

void DiceWebSigninInterceptionBackdropLayer::OnViewBoundsChanged(
    views::View* observed_view) {
  CHECK_EQ(observed_view, highlighted_button_);
  SchedulePaint();
}

void DiceWebSigninInterceptionBackdropLayer::OnViewIsDeleting(
    views::View* observed_view) {
  CHECK_EQ(observed_view, highlighted_button_);
  highlighted_button_ = nullptr;
  highlighted_button_observation_.Reset();
}

void DiceWebSigninInterceptionBackdropLayer::OnWidgetDestroying(
    views::Widget* widget) {
  browser_frame_observation_.Reset();
}

void DiceWebSigninInterceptionBackdropLayer::OnWidgetBoundsChanged(
    views::Widget* widget,
    const gfx::Rect& new_bounds) {
  UpdateLayerBounds();
  SchedulePaint();
}
