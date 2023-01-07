// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_header_chromeos.h"

#include "base/check.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chromeos/ui/base/chromeos_ui_constants.h"
#include "chromeos/ui/base/tablet_state.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/frame/caption_buttons/frame_caption_button_container_view.h"
#include "chromeos/ui/frame/frame_utils.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/view.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/caption_button_layout_constants.h"

namespace {

void PaintThemedFrame(gfx::Canvas* canvas,
                      const gfx::ImageSkia& frame_image,
                      const gfx::ImageSkia& frame_overlay_image,
                      SkColor background_color,
                      const gfx::Rect& bounds,
                      int image_inset_x,
                      int image_inset_y) {
  SkColor opaque_background_color =
      SkColorSetA(background_color, SK_AlphaOPAQUE);

  // When no images are used, just draw a color.
  if (frame_image.isNull() && frame_overlay_image.isNull()) {
    canvas->DrawColor(opaque_background_color);
    return;
  }

  // This handles the case where blending is required between one or more images
  // and the background color. In this case we use a SaveLayerWithFlags() call
  // to draw all 2-3 components into a single layer.
  const bool blending_required =
      !frame_image.isNull() && !frame_overlay_image.isNull();
  if (blending_required) {
    cc::PaintFlags flags;
    // We use kPlus blending mode so that between the active and inactive
    // background colors, the result is 255 alpha (i.e. opaque).
    flags.setBlendMode(SkBlendMode::kPlus);
    canvas->SaveLayerWithFlags(flags);
  }

  // Images can be transparent and we expect the background color to be present
  // behind them.
  canvas->DrawColor(opaque_background_color);
  if (!frame_image.isNull()) {
    canvas->TileImageInt(frame_image, image_inset_x, image_inset_y, 0, 0,
                         bounds.width(), bounds.height(), 1.0f,
                         SkTileMode::kRepeat, SkTileMode::kMirror);
  }
  if (!frame_overlay_image.isNull())
    canvas->DrawImageInt(frame_overlay_image, 0, 0);

  if (blending_required)
    canvas->Restore();
}

// Returns the frame path with the given |bounds| and |corner_radius|
// for the rounded corner of the frame header.
SkPath GetFrameHeaderPath(const gfx::Rect& bounds, int corner_radius) {
  const SkScalar sk_corner_radius = SkIntToScalar(corner_radius);
  const SkScalar radii[8] = {sk_corner_radius,
                             sk_corner_radius,  // top-left
                             sk_corner_radius,
                             sk_corner_radius,  // top-right
                             0,
                             0,  // bottom-right
                             0,
                             0};  // bottom-left
  SkPath frame_path;
  frame_path.addRoundRect(gfx::RectToSkRect(bounds), radii,
                          SkPathDirection::kCW);
  return frame_path;
}

// Tiles |frame_image| and |frame_overlay_image| into an area, rounding the top
// corners.
void PaintFrameImagesInRoundRect(gfx::Canvas* canvas,
                                 const gfx::ImageSkia& frame_image,
                                 const gfx::ImageSkia& frame_overlay_image,
                                 SkColor background_color,
                                 const gfx::Rect& bounds,
                                 int image_inset_x,
                                 int image_inset_y,
                                 int corner_radius) {
  bool antialias = corner_radius > 0;

  gfx::ScopedCanvas scoped_save(canvas);
  canvas->ClipPath(GetFrameHeaderPath(bounds, corner_radius), antialias);

  PaintThemedFrame(canvas, frame_image, frame_overlay_image, background_color,
                   bounds, image_inset_x, image_inset_y);
}

int GetCornerRadius(chromeos::WindowStateType state_type) {
  return chromeos::IsNormalWindowStateType(state_type)
             ? chromeos::kTopCornerRadiusWhenRestored
             : 0;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameHeaderChromeOS, public:

BrowserFrameHeaderChromeOS::BrowserFrameHeaderChromeOS(
    views::Widget* target_widget,
    views::View* view,
    AppearanceProvider* appearance_provider,
    chromeos::FrameCaptionButtonContainerView* caption_button_container)
    : FrameHeader(target_widget, view) {
  DCHECK(appearance_provider);
  DCHECK(caption_button_container);
  appearance_provider_ = appearance_provider;

  SetCaptionButtonContainer(caption_button_container);
}

BrowserFrameHeaderChromeOS::~BrowserFrameHeaderChromeOS() = default;

// static
int BrowserFrameHeaderChromeOS::GetThemeBackgroundXInset() {
  // In the pre-Ash era the web content area had a frame along the left edge, so
  // user-generated theme images for the new tab page assume they are shifted
  // right relative to the header.  Now that we have removed the left edge frame
  // we need to copy the theme image for the window header from a few pixels
  // inset to preserve alignment with the NTP image, or else we'll break a bunch
  // of existing themes.  We do something similar on OS X for the same reason.
  return 5;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameHeaderChromeOS, protected:

void BrowserFrameHeaderChromeOS::DoPaintHeader(gfx::Canvas* canvas) {
  PaintFrameImages(canvas);
  PaintTitleBar(canvas);
}

views::CaptionButtonLayoutSize BrowserFrameHeaderChromeOS::GetButtonLayoutSize()
    const {
  if (chromeos::TabletState::Get() &&
      chromeos::TabletState::Get()->InTabletMode())
    return views::CaptionButtonLayoutSize::kBrowserCaptionMaximized;

  return chromeos::ShouldUseRestoreFrame(target_widget())
             ? views::CaptionButtonLayoutSize::kBrowserCaptionRestored
             : views::CaptionButtonLayoutSize::kBrowserCaptionMaximized;
}

SkColor BrowserFrameHeaderChromeOS::GetTitleColor() const {
  return appearance_provider_->GetTitleColor();
}

SkColor BrowserFrameHeaderChromeOS::GetCurrentFrameColor() const {
  return appearance_provider_->GetFrameHeaderColor(mode() == MODE_ACTIVE);
}

void BrowserFrameHeaderChromeOS::UpdateFrameColors() {
  SetPaintAsActive(target_widget()->ShouldPaintAsActive());
  UpdateCaptionButtonColors();
  view()->SchedulePaint();
}

SkPath BrowserFrameHeaderChromeOS::GetWindowMaskForFrameHeader(
    const gfx::Size& size) {
  chromeos::WindowStateType state_type =
      target_widget()->GetNativeWindow()->GetProperty(
          chromeos::kWindowStateTypeKey);
  return GetFrameHeaderPath(gfx::Rect(size), GetCornerRadius(state_type));
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameHeaderChromeOS, private:

void BrowserFrameHeaderChromeOS::PaintFrameImages(gfx::Canvas* canvas) {
  const bool active = mode() == Mode::MODE_ACTIVE;

  gfx::ImageSkia frame_image =
      appearance_provider_->GetFrameHeaderImage(active);
  gfx::ImageSkia frame_overlay_image =
      appearance_provider_->GetFrameHeaderOverlayImage(active);

  chromeos::WindowStateType state_type =
      target_widget()->GetNativeWindow()->GetProperty(
          chromeos::kWindowStateTypeKey);

  PaintFrameImagesInRoundRect(canvas, frame_image, frame_overlay_image,
                              appearance_provider_->GetFrameHeaderColor(active),
                              GetPaintedBounds(), GetThemeBackgroundXInset(),
                              appearance_provider_->GetFrameHeaderImageYInset(),
                              GetCornerRadius(state_type));
}
