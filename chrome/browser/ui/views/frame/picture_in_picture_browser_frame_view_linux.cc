// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/picture_in_picture_browser_frame_view_linux.h"

#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_frame_view_paint_utils_linux.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/desktop_browser_frame_aura_linux.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/linux/linux_ui.h"
#include "ui/views/window/frame_background.h"

namespace {

// Frame border when window shadow is not drawn.
constexpr int kFrameBorderThickness = 4;

}  // namespace

gfx::ShadowValues PictureInPictureBrowserFrameViewLinux::GetShadowValues() {
  int elevation = ChromeLayoutProvider::Get()->GetShadowElevationMetric(
      views::Emphasis::kMaximum);
  return gfx::ShadowValue::MakeMdShadowValues(elevation);
}

PictureInPictureBrowserFrameViewLinux::PictureInPictureBrowserFrameViewLinux(
    BrowserFrame* frame,
    BrowserView* browser_view)
    : PictureInPictureBrowserFrameView(frame, browser_view) {
  auto* profile = browser_view->browser()->profile();
  auto* linux_ui_theme = ui::LinuxUiTheme::GetForProfile(profile);
  auto* theme_service_factory = ThemeServiceFactory::GetForProfile(profile);
  if (linux_ui_theme && theme_service_factory->UsingSystemTheme()) {
    bool solid_frame = !static_cast<DesktopBrowserFrameAuraLinux*>(
                            frame->native_browser_frame())
                            ->ShouldDrawRestoredFrameShadow();

    // This may return null, but that's handled below.
    window_frame_provider_ = linux_ui_theme->GetWindowFrameProvider(
        solid_frame, /*tiled=*/false,
        /*maximized=*/frame->IsMaximized());
  }

  // On Linux the top bar background will be drawn in OnPaint().
  top_bar_container_view()->SetBackground(nullptr);

  // Only one of window_frame_provider_ and frame_background_ will be used.
  if (!window_frame_provider_) {
    frame_background_ = std::make_unique<views::FrameBackground>();
  }
}

PictureInPictureBrowserFrameViewLinux::
    ~PictureInPictureBrowserFrameViewLinux() = default;

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameView: implementations:

gfx::Insets
PictureInPictureBrowserFrameViewLinux::RestoredMirroredFrameBorderInsets()
    const {
  auto border = FrameBorderInsets();
  return base::i18n::IsRTL() ? gfx::Insets::TLBR(border.top(), border.right(),
                                                 border.bottom(), border.left())
                             : border;
}

gfx::Insets PictureInPictureBrowserFrameViewLinux::GetInputInsets() const {
  return ShouldDrawFrameShadow() ? ResizeBorderInsets() : gfx::Insets();
}

SkRRect PictureInPictureBrowserFrameViewLinux::GetRestoredClipRegion() const {
  gfx::RectF bounds_dip(GetLocalBounds());
  if (ShouldDrawFrameShadow()) {
    gfx::InsetsF border(RestoredMirroredFrameBorderInsets());
    bounds_dip.Inset(border);
  }

  float radius_dip = 0;
  if (window_frame_provider_) {
    radius_dip = window_frame_provider_->GetTopCornerRadiusDip();
  } else {
    radius_dip = ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
        views::Emphasis::kHigh);
  }
  SkVector radii[4]{{radius_dip, radius_dip}, {radius_dip, radius_dip}, {}, {}};
  SkRRect clip;
  clip.setRectRadii(gfx::RectFToSkRect(bounds_dip), radii);
  return clip;
}

///////////////////////////////////////////////////////////////////////////////
// PictureInPictureBrowserFrameView implementations:

gfx::Rect PictureInPictureBrowserFrameViewLinux::GetHitRegion() const {
  gfx::Rect hit_region = GetLocalBounds();
  if (ShouldDrawFrameShadow()) {
    gfx::Insets insets = RestoredMirroredFrameBorderInsets();
    if (frame()->tiled()) {
      insets = gfx::Insets();
    }

    hit_region.Inset(insets - GetInputInsets());
  }

  return hit_region;
}

///////////////////////////////////////////////////////////////////////////////
// views::View implementations:

void PictureInPictureBrowserFrameViewLinux::OnPaint(gfx::Canvas* canvas) {
  // Draw the PiP window frame borders and shadows, including the top bar
  // background.
  if (window_frame_provider_) {
    window_frame_provider_->PaintWindowFrame(
        canvas, GetLocalBounds(), GetTopAreaHeight(), ShouldPaintAsActive(),
        GetInputInsets());
  } else {
    CHECK(frame_background_);
    frame_background_->set_frame_color(
        GetColorProvider()->GetColor(kColorPipWindowTopBarBackground));
    frame_background_->set_use_custom_frame(frame()->UseCustomFrame());
    frame_background_->set_is_active(ShouldPaintAsActive());
    frame_background_->set_theme_image(GetFrameImage());

    frame_background_->set_theme_image_inset(
        browser_view()->GetThemeOffsetFromBrowserView());
    frame_background_->set_theme_overlay_image(GetFrameOverlayImage());
    frame_background_->set_top_area_height(GetTopAreaHeight());
    PaintRestoredFrameBorderLinux(
        *canvas, *this, frame_background_.get(), GetRestoredClipRegion(),
        ShouldDrawFrameShadow(), ShouldPaintAsActive(),
        RestoredMirroredFrameBorderInsets(), GetShadowValues(),
        frame()->tiled());
  }

  BrowserNonClientFrameView::OnPaint(canvas);
}

bool PictureInPictureBrowserFrameViewLinux::ShouldDrawFrameShadow() const {
  return static_cast<DesktopBrowserFrameAuraLinux*>(
             frame()->native_browser_frame())
      ->ShouldDrawRestoredFrameShadow();
}

///////////////////////////////////////////////////////////////////////////////
// PictureInPictureBrowserFrameView: implementations:

gfx::Insets PictureInPictureBrowserFrameViewLinux::ResizeBorderInsets() const {
  return FrameBorderInsets();
}

gfx::Insets PictureInPictureBrowserFrameViewLinux::FrameBorderInsets() const {
  if (window_frame_provider_) {
    const auto insets = window_frame_provider_->GetFrameThicknessDip();
    const bool tiled = frame()->tiled();

    // If edges of the window are tiled and snapped to the edges of the desktop,
    // window_frame_provider_ will skip drawing.
    return tiled ? gfx::Insets() : insets;
  }

  return GetRestoredFrameBorderInsetsLinux(
      ShouldDrawFrameShadow(), gfx::Insets(kFrameBorderThickness),
      GetShadowValues(),
      PictureInPictureBrowserFrameView::ResizeBorderInsets());
}
