// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_view_linux.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/desktop_browser_frame_aura_linux.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/window/frame_background.h"
#include "ui/views/window/window_button_order_provider.h"

BrowserFrameViewLinux::BrowserFrameViewLinux(
    BrowserFrame* frame,
    BrowserView* browser_view,
    BrowserFrameViewLayoutLinux* layout)
    : OpaqueBrowserFrameView(frame, browser_view, layout), layout_(layout) {
  layout->set_view(this);
  if (views::LinuxUI* ui = views::LinuxUI::instance()) {
    ui->AddWindowButtonOrderObserver(this);
    OnWindowButtonOrderingChange();
  }
}

BrowserFrameViewLinux::~BrowserFrameViewLinux() {
  if (views::LinuxUI* ui = views::LinuxUI::instance())
    ui->RemoveWindowButtonOrderObserver(this);
}

SkRRect BrowserFrameViewLinux::GetRestoredClipRegion() const {
  gfx::RectF bounds_dip(GetLocalBounds());
  if (ShouldDrawRestoredFrameShadow()) {
    gfx::InsetsF border(layout_->MirroredFrameBorderInsets());
    bounds_dip.Inset(border);
  }
  float radius_dip = GetRestoredCornerRadiusDip();
  SkVector radii[4]{{radius_dip, radius_dip}, {radius_dip, radius_dip}, {}, {}};
  SkRRect clip;
  clip.setRectRadii(gfx::RectFToSkRect(bounds_dip), radii);
  return clip;
}

// static
gfx::ShadowValues BrowserFrameViewLinux::GetShadowValues() {
  int elevation = ChromeLayoutProvider::Get()->GetShadowElevationMetric(
      views::Emphasis::kMaximum);
  return gfx::ShadowValue::MakeMdShadowValues(elevation);
}

void BrowserFrameViewLinux::OnWindowButtonOrderingChange() {
  auto* provider = views::WindowButtonOrderProvider::GetInstance();
  layout_->SetButtonOrdering(provider->leading_buttons(),
                             provider->trailing_buttons());

  // We can receive OnWindowButtonOrderingChange events before we've been added
  // to a Widget. We need a Widget because layout crashes due to dependencies
  // on a ui::ThemeProvider().
  if (auto* widget = GetWidget()) {
    // A relayout on |view_| is insufficient because it would neglect
    // a relayout of the tabstrip.  Do a full relayout to handle the
    // frame buttons as well as open tabs.
    views::View* root_view = widget->GetRootView();
    root_view->Layout();
    root_view->SchedulePaint();
  }
}

void BrowserFrameViewLinux::PaintRestoredFrameBorder(
    gfx::Canvas* canvas) const {
  auto clip = GetRestoredClipRegion();
  bool showing_shadow = ShouldDrawRestoredFrameShadow();

  if (auto* frame_bg = frame_background()) {
    gfx::ScopedCanvas scoped_canvas(canvas);
    canvas->sk_canvas()->clipRRect(clip, SkClipOp::kIntersect, true);
    auto border = layout_->MirroredFrameBorderInsets();
    auto shadow_inset = showing_shadow ? border : gfx::Insets();
    frame_bg->PaintMaximized(canvas, GetNativeTheme(), GetColorProvider(),
                             shadow_inset.left(), shadow_inset.top(),
                             width() - shadow_inset.width());
    if (!showing_shadow)
      frame_bg->FillFrameBorders(canvas, this, border.left(), border.right(),
                                 border.bottom());
  }

  // If rendering shadows, draw a 1px exterior border, otherwise
  // draw a 1px interior border.
  const SkScalar one_pixel = SkFloatToScalar(1 / canvas->image_scale());
  auto rect = clip;
  if (showing_shadow)
    rect.outset(one_pixel, one_pixel);
  else
    clip.inset(one_pixel, one_pixel);

  cc::PaintFlags flags;
  flags.setColor(GetColorProvider()->GetColor(
      showing_shadow ? ui::kColorBubbleBorderWhenShadowPresent
                     : ui::kColorBubbleBorder));
  flags.setAntiAlias(true);
  if (showing_shadow)
    flags.setLooper(gfx::CreateShadowDrawLooper(GetShadowValues()));

  gfx::ScopedCanvas scoped_canvas(canvas);
  canvas->sk_canvas()->clipRRect(clip, SkClipOp::kDifference, true);
  canvas->sk_canvas()->drawRRect(rect, flags);
}

void BrowserFrameViewLinux::GetWindowMask(const gfx::Size& size,
                                          SkPath* window_mask) {
  // This class uses transparency to draw rounded corners, so a
  // window mask is not necessary.
}

bool BrowserFrameViewLinux::ShouldDrawRestoredFrameShadow() const {
  return static_cast<DesktopBrowserFrameAuraLinux*>(
             frame()->native_browser_frame())
      ->ShouldDrawRestoredFrameShadow();
}

float BrowserFrameViewLinux::GetRestoredCornerRadiusDip() const {
  if (!UseCustomFrame() || !IsTranslucentWindowOpacitySupported())
    return 0;
  return ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kHigh);
}
