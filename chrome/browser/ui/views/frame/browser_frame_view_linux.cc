// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_view_linux.h"

#include "base/notreached.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_frame_view_paint_utils_linux.h"
#include "chrome/browser/ui/views/frame/browser_native_widget_aura_linux.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/base/hit_test.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/shadow_value.h"
#include "ui/linux/linux_ui.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/window/window_button_order_provider.h"

namespace {

// The resize border at the top of the caption area. Only used when frame
// shadows are disabled. The value is chosen to match the left, right, and
// bottom resize borders.
constexpr int kResizeTopBorderThickness = 4;

}  // namespace

BrowserFrameViewLinux::BrowserFrameViewLinux(
    BrowserWidget* widget,
    BrowserView* browser_view,
    BrowserFrameViewLayoutLinux* layout)
    : OpaqueBrowserFrameView(widget, browser_view, layout), layout_(layout) {
  layout->set_view(this);
  if (auto* linux_ui = ui::LinuxUi::instance()) {
    window_button_order_observation_.Observe(linux_ui);
    OnWindowButtonOrderingChange();
  }
}

BrowserFrameViewLinux::~BrowserFrameViewLinux() = default;

gfx::Insets BrowserFrameViewLinux::RestoredMirroredFrameBorderInsets() const {
  return layout_->RestoredMirroredFrameBorderInsets();
}

gfx::Insets BrowserFrameViewLinux::GetInputInsets() const {
  return layout_->GetInputInsets();
}

SkRRect BrowserFrameViewLinux::GetRestoredClipRegion() const {
  gfx::RectF bounds_dip(GetLocalBounds());
  if (ShouldDrawRestoredFrameShadow()) {
    gfx::InsetsF border(layout_->RestoredMirroredFrameBorderInsets());
    bounds_dip.Inset(border);
  }
  float radius_dip = GetRestoredCornerRadiusDip();
  SkVector radii[4]{{radius_dip, radius_dip}, {radius_dip, radius_dip}, {}, {}};
  SkRRect clip;
  clip.setRectRadii(gfx::RectFToSkRect(bounds_dip), radii);
  return clip;
}

// static
gfx::ShadowValues BrowserFrameViewLinux::GetShadowValues(bool active) {
  int elevation = ChromeLayoutProvider::Get()->GetShadowElevationMetric(
      active ? views::Emphasis::kMaximum : views::Emphasis::kMedium);
  return gfx::ShadowValue::MakeMdShadowValues(elevation);
}

void BrowserFrameViewLinux::PaintRestoredFrameBorder(
    gfx::Canvas* canvas) const {
#if BUILDFLAG(IS_LINUX)
  const bool tiled = browser_widget()->tiled();
#else
  const bool tiled = false;
#endif
  auto shadow_values =
      tiled ? gfx::ShadowValues() : GetShadowValues(ShouldPaintAsActive());
  PaintRestoredFrameBorderLinux(
      *canvas, *this, frame_background(), GetRestoredClipRegion(),
      ShouldDrawRestoredFrameShadow(), ShouldPaintAsActive(),
      layout_->RestoredMirroredFrameBorderInsets(), shadow_values, tiled);
}

void BrowserFrameViewLinux::GetWindowMask(const gfx::Size& size,
                                          SkPath* window_mask) {
  // This class uses transparency to draw rounded corners, so a
  // window mask is not necessary.
}

bool BrowserFrameViewLinux::ShouldDrawRestoredFrameShadow() const {
  return static_cast<BrowserNativeWidgetAuraLinux*>(
             browser_widget()->browser_native_widget())
      ->ShouldDrawRestoredFrameShadow();
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
    root_view->DeprecatedLayoutImmediately();
    root_view->SchedulePaint();
  }
}

int BrowserFrameViewLinux::NonClientHitTest(const gfx::Point& point) {
  int frame_component = OpaqueBrowserFrameView::NonClientHitTest(point);
  // Allow resizing at the top of the caption area. This is only done when
  // shadows are not drawn, since the resize area is on the shadows otherwise.
  if (frame_component == HTCAPTION && !ShouldDrawRestoredFrameShadow() &&
      !IsFrameCondensed() && point.y() < kResizeTopBorderThickness) {
    return HTTOP;
  }
  return frame_component;
}

float BrowserFrameViewLinux::GetRestoredCornerRadiusDip() const {
#if BUILDFLAG(IS_LINUX)
  const bool tiled = browser_widget()->tiled();
#else
  const bool tiled = false;
#endif
  if (tiled || !UseCustomFrame() ||
      !views::Widget::IsWindowCompositingSupported()) {
    return 0;
  }
  return ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kHigh);
}

int BrowserFrameViewLinux::GetTranslucentTopAreaHeight() const {
  return 0;
}

void BrowserFrameViewLinux::LayoutWebAppWindowTitle(
    const gfx::Rect& available_space,
    views::Label& window_title_label) const {
  constexpr int kIconTitleSpacing = 4;
  constexpr int kCaptionSpacing = 5;

  gfx::Rect bounds = available_space;
  bounds.Inset(gfx::Insets::TLBR(0, kIconTitleSpacing, 0, kCaptionSpacing));
  window_title_label.SetSubpixelRenderingEnabled(false);
  window_title_label.SetHorizontalAlignment(gfx::ALIGN_LEFT);
  window_title_label.SetBoundsRect(bounds);
}

BrowserLayoutParams BrowserFrameViewLinux::GetBrowserLayoutParams() const {
  BrowserLayoutParams params;
  params.visual_client_area = GetBoundsForClientView();

  // Some opaque frames add small margins next to the caption buttons.
  const int caption_margin =
      layout_->GetWindowCaptionSpacing(views::FrameButton::kMinimize,
                                       /*leading_spacing=*/false,
                                       /*is_leading_button=*/false);

  // On Linux, buttons may be split between leading and trailing.
  // Account for both in the exclusion areas.

  auto* const provider = views::WindowButtonOrderProvider::GetInstance();

  gfx::Rect leading_bounds;
  for (auto button : provider->leading_buttons()) {
    if (auto* const button_view = layout_->GetFrameButton(button);
        button_view && button_view->GetVisible()) {
      leading_bounds.Union(button_view->bounds());
    }
  }
  if (!leading_bounds.IsEmpty()) {
    params.leading_exclusion.content =
        gfx::SizeF(leading_bounds.right() - params.visual_client_area.x(),
                   leading_bounds.bottom() - params.visual_client_area.y());
    params.leading_exclusion.horizontal_padding = caption_margin;
  }

  gfx::Rect trailing_bounds;
  for (auto button : provider->trailing_buttons()) {
    if (auto* const button_view = layout_->GetFrameButton(button);
        button_view && button_view->GetVisible()) {
      trailing_bounds.Union(button_view->bounds());
    }
  }
  if (!trailing_bounds.IsEmpty()) {
    params.trailing_exclusion.content =
        gfx::SizeF(params.visual_client_area.right() - trailing_bounds.x(),
                   trailing_bounds.bottom() - params.visual_client_area.y());
    params.trailing_exclusion.horizontal_padding = caption_margin;
  }

  MaybeAddAppIconToLayoutParams(params);
  return params;
}

bool BrowserFrameViewLinux::CaptionButtonsOnLeadingEdge() const {
  auto* const provider = views::WindowButtonOrderProvider::GetInstance();
  return !provider->leading_buttons().empty();
}

bool BrowserFrameViewLinux::CaptionButtonsOnTrailingEdge() const {
  auto* const provider = views::WindowButtonOrderProvider::GetInstance();
  return !provider->trailing_buttons().empty();
}

BrowserFrameViewLinux::BoundsAndMargins
BrowserFrameViewLinux::GetCaptionButtonBounds() const {
  NOTREACHED() << "Linux uses a different computation for caption buttons.";
}

BEGIN_METADATA(BrowserFrameViewLinux)
END_METADATA
