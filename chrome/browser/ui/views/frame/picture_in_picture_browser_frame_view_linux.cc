// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/picture_in_picture_browser_frame_view_linux.h"

#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_aura_linux.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_native_widget_aura_linux.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/linux/linux_ui.h"
#include "ui/linux/window_frame_provider.h"
#include "ui/views/window/frame_background.h"
#include "ui/views/window/frame_view_utils_linux.h"

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
    BrowserWidget* widget,
    BrowserView* browser_view)
    : PictureInPictureBrowserFrameView(widget, browser_view) {
  auto* profile = browser_view->browser()->GetProfile();
  auto* linux_ui_theme =
      ThemeServiceAuraLinux::GetLinuxUiThemeForProfile(profile);
  auto* theme_service_factory = ThemeServiceFactory::GetForProfile(profile);
  if (linux_ui_theme && theme_service_factory->UsingSystemTheme()) {
    bool solid_frame = !static_cast<BrowserNativeWidgetAuraLinux*>(
                            widget->browser_native_widget())
                            ->ShouldDrawRestoredFrameShadow();

    // These may return null, but that's handled below.
    window_frame_provider_ = linux_ui_theme->GetWindowFrameProvider(
        ui::FrameType::kBrowser, solid_frame, /*tiled=*/false,
        /*maximized=*/false);
    maximized_window_frame_provider_ = linux_ui_theme->GetWindowFrameProvider(
        ui::FrameType::kBrowser, solid_frame, /*tiled=*/false,
        /*maximized=*/true);
    CHECK_EQ(!!window_frame_provider_, !!maximized_window_frame_provider_);
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
// BrowserFrameView: implementations:

gfx::Insets
PictureInPictureBrowserFrameViewLinux::RestoredMirroredFrameBorderInsets()
    const {
  auto border = RestoredFrameBorderInsets();
  return base::i18n::IsRTL() ? gfx::Insets::TLBR(border.top(), border.right(),
                                                 border.bottom(), border.left())
                             : border;
}

gfx::Insets PictureInPictureBrowserFrameViewLinux::GetInputInsets() const {
  return ShouldDrawFrameShadow() ? ResizeBorderInsets() : gfx::Insets();
}

SkRRect PictureInPictureBrowserFrameViewLinux::GetRestoredClipRegion() const {
  gfx::InsetsF border = ShouldDrawFrameShadow()
                            ? gfx::InsetsF(RestoredMirroredFrameBorderInsets())
                            : gfx::InsetsF();
  float radius_dip = 0;
  if (window_frame_provider_) {
    radius_dip = window_frame_provider_->GetTopCornerRadiusDip();
  } else {
    radius_dip = ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
        views::Emphasis::kHigh);
  }
  return views::GetRestoredClipRegion(
      gfx::RectF(GetLocalBounds()), border,
      gfx::RoundedCornersF(radius_dip, radius_dip, 0, 0));
}

///////////////////////////////////////////////////////////////////////////////
// views::FrameView implementations:
gfx::Rect
PictureInPictureBrowserFrameViewLinux::GetNonDecoratedClientAreaBoundsInScreen()
    const {
  gfx::Rect bounds = GetLocalBounds();
  views::View::ConvertRectToScreen(this, &bounds);

  if (ShouldDrawFrameShadow()) {
    bounds.Inset(FrameBorderInsets());
  }

  return bounds;
}

///////////////////////////////////////////////////////////////////////////////
// PictureInPictureBrowserFrameView implementations:

gfx::Rect PictureInPictureBrowserFrameViewLinux::GetHitRegion() const {
  gfx::Rect hit_region = GetLocalBounds();
  if (ShouldDrawFrameShadow()) {
    gfx::Insets insets = RestoredMirroredFrameBorderInsets();
    if (browser_widget()->tiled() || !IsShowingFrame()) {
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
  // background. Maximized and fullscreen windows have no frame border or
  // shadow; only the top bar background is drawn.
  const bool showing_frame = IsShowingFrame();
  if (auto* provider = showing_frame ? window_frame_provider_.get()
                                     : maximized_window_frame_provider_.get()) {
    provider->PaintWindowFrame(canvas, GetLocalBounds(), GetTopAreaHeight(),
                               ShouldPaintAsActive(), GetInputInsets());
  } else {
    CHECK(frame_background_);
    frame_background_->set_frame_color(
        GetColorProvider()->GetColor(kColorPipWindowTopBarBackground));
    frame_background_->set_use_custom_frame(
        browser_widget()->browser_native_widget()->UseCustomFrame());
    frame_background_->set_is_active(ShouldPaintAsActive());
    frame_background_->set_theme_image(GetFrameImage());

    frame_background_->set_theme_image_inset(
        GetBrowserView()->GetThemeOffsetFromBrowserView());
    frame_background_->set_theme_overlay_image(GetFrameOverlayImage());
    frame_background_->set_top_area_height(GetTopAreaHeight());
    if (showing_frame) {
      views::PaintRestoredFrameBorderLinux(
          *canvas, *this, frame_background_.get(), GetRestoredClipRegion(),
          ShouldDrawFrameShadow(), ShouldPaintAsActive(),
          RestoredMirroredFrameBorderInsets(), GetShadowValues(),
          browser_widget()->tiled());
    } else {
      frame_background_->PaintMaximized(canvas, GetNativeTheme(),
                                        GetColorProvider(), 0, 0, width());
    }
  }

  BrowserFrameView::OnPaint(canvas);
}

///////////////////////////////////////////////////////////////////////////////
// views::WidgetObserver implementations:

void PictureInPictureBrowserFrameViewLinux::OnWidgetShowStateChanged(
    views::Widget* widget) {
  // The frame insets and the frame provider depend on the window state.
  InvalidateLayout();
  SchedulePaint();
}

bool PictureInPictureBrowserFrameViewLinux::ShouldDrawFrameShadow() const {
  return static_cast<BrowserNativeWidgetAuraLinux*>(
             browser_widget()->browser_native_widget())
      ->ShouldDrawRestoredFrameShadow();
}

///////////////////////////////////////////////////////////////////////////////
// PictureInPictureBrowserFrameView: implementations:

gfx::Insets PictureInPictureBrowserFrameViewLinux::ResizeBorderInsets() const {
  return FrameBorderInsets();
}

gfx::Insets PictureInPictureBrowserFrameViewLinux::FrameBorderInsets() const {
  // Like the regular browser frame, maximized and fullscreen windows have no
  // frame border. BrowserDesktopWindowTreeHostLinux::CalculateInsetsInDIP()
  // also drops the insets from the window geometry in these states.
  return IsShowingFrame() ? RestoredFrameBorderInsets() : gfx::Insets();
}

///////////////////////////////////////////////////////////////////////////////
// PictureInPictureBrowserFrameViewLinux, private:

gfx::Insets PictureInPictureBrowserFrameViewLinux::RestoredFrameBorderInsets()
    const {
  if (window_frame_provider_) {
    const auto insets = window_frame_provider_->GetFrameThicknessDip();
    const bool tiled = browser_widget()->tiled();

    // If edges of the window are tiled and snapped to the edges of the desktop,
    // window_frame_provider_ will skip drawing.
    return tiled ? gfx::Insets() : insets;
  }

  return views::GetRestoredFrameBorderInsetsLinux(
      ShouldDrawFrameShadow(), gfx::Insets(kFrameBorderThickness),
      GetShadowValues(),
      PictureInPictureBrowserFrameView::ResizeBorderInsets());
}

bool PictureInPictureBrowserFrameViewLinux::IsShowingFrame() const {
  return !browser_widget()->IsMaximized() && !browser_widget()->IsFullscreen();
}
