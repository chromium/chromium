// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_header_ash.h"

#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/caption_buttons/frame_caption_button_container_view.h"
#include "ash/public/cpp/frame_utils.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/public/cpp/window_state_type.h"
#include "base/check.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
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
                      int image_inset_y,
                      int alpha) {
  SkColor opaque_background_color =
      SkColorSetA(background_color, SK_AlphaOPAQUE);

  // When no images are used, just draw a color, with the animation |alpha|
  // applied.
  if (frame_image.isNull() && frame_overlay_image.isNull()) {
    // We use kPlus blending mode so that between the active and inactive
    // background colors, the result is 255 alpha (i.e. opaque).
    canvas->DrawColor(SkColorSetA(opaque_background_color, alpha),
                      SkBlendMode::kPlus);
    return;
  }

  // This handles the case where blending is required between one or more images
  // and the background color. In this case we use a SaveLayerWithFlags() call
  // to draw all 2-3 components into a single layer then apply the alpha to them
  // together.
  const bool blending_required =
      alpha < 0xFF || (!frame_image.isNull() && !frame_overlay_image.isNull());
  if (blending_required) {
    cc::PaintFlags flags;
    // We use kPlus blending mode so that between the active and inactive
    // background colors, the result is 255 alpha (i.e. opaque).
    flags.setBlendMode(SkBlendMode::kPlus);
    flags.setAlpha(alpha);
    canvas->SaveLayerWithFlags(flags);
  }

  // Images can be transparent and we expect the background color to be present
  // behind them. Here the |alpha| will be applied to the background color by
  // the SaveLayer call, so use |opaque_background_color|.
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

// Tiles |frame_image| and |frame_overlay_image| into an area, rounding the top
// corners.
void PaintFrameImagesInRoundRect(gfx::Canvas* canvas,
                                 const gfx::ImageSkia& frame_image,
                                 const gfx::ImageSkia& frame_overlay_image,
                                 SkColor background_color,
                                 const gfx::Rect& bounds,
                                 int image_inset_x,
                                 int image_inset_y,
                                 int alpha,
                                 int corner_radius) {
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
  bool antialias = corner_radius > 0;

  gfx::ScopedCanvas scoped_save(canvas);
  canvas->ClipPath(frame_path, antialias);

  PaintThemedFrame(canvas, frame_image, frame_overlay_image, background_color,
                   bounds, image_inset_x, image_inset_y, alpha);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameHeaderAsh, public:

BrowserFrameHeaderAsh::BrowserFrameHeaderAsh(
    views::Widget* target_widget,
    views::View* view,
    AppearanceProvider* appearance_provider,
    ash::FrameCaptionButtonContainerView* caption_button_container)
    : FrameHeader(target_widget, view) {
  DCHECK(appearance_provider);
  DCHECK(caption_button_container);
  appearance_provider_ = appearance_provider;

  SetCaptionButtonContainer(caption_button_container);
}

BrowserFrameHeaderAsh::~BrowserFrameHeaderAsh() = default;

// static
int BrowserFrameHeaderAsh::GetThemeBackgroundXInset() {
  // In the pre-Ash era the web content area had a frame along the left edge, so
  // user-generated theme images for the new tab page assume they are shifted
  // right relative to the header.  Now that we have removed the left edge frame
  // we need to copy the theme image for the window header from a few pixels
  // inset to preserve alignment with the NTP image, or else we'll break a bunch
  // of existing themes.  We do something similar on OS X for the same reason.
  return 5;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameHeaderAsh, protected:

void BrowserFrameHeaderAsh::DoPaintHeader(gfx::Canvas* canvas) {
  PaintFrameImages(canvas, false /* active */);
  PaintFrameImages(canvas, true /* active */);
  PaintTitleBar(canvas);
}

views::CaptionButtonLayoutSize BrowserFrameHeaderAsh::GetButtonLayoutSize()
    const {
  if (ash::TabletMode::Get() && ash::TabletMode::Get()->InTabletMode())
    return views::CaptionButtonLayoutSize::kBrowserCaptionMaximized;

  return ash::ShouldUseRestoreFrame(target_widget())
             ? views::CaptionButtonLayoutSize::kBrowserCaptionRestored
             : views::CaptionButtonLayoutSize::kBrowserCaptionMaximized;
}

SkColor BrowserFrameHeaderAsh::GetTitleColor() const {
  return appearance_provider_->GetTitleColor();
}

SkColor BrowserFrameHeaderAsh::GetCurrentFrameColor() const {
  return appearance_provider_->GetFrameHeaderColor(mode() == MODE_ACTIVE);
}

void BrowserFrameHeaderAsh::UpdateFrameColors() {
  UpdateCaptionButtonColors();
  view()->SchedulePaint();
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameHeaderAsh, private:

void BrowserFrameHeaderAsh::PaintFrameImages(gfx::Canvas* canvas, bool active) {
  int alpha = activation_animation().CurrentValueBetween(0, 0xFF);
  if (!active)
    alpha = 0xFF - alpha;

  if (alpha == 0)
    return;

  gfx::ImageSkia frame_image =
      appearance_provider_->GetFrameHeaderImage(active);
  gfx::ImageSkia frame_overlay_image =
      appearance_provider_->GetFrameHeaderOverlayImage(active);

  ash::WindowStateType state_type =
      target_widget()->GetNativeWindow()->GetProperty(ash::kWindowStateTypeKey);
  int corner_radius = ash::IsNormalWindowStateType(state_type)
                          ? ash::kTopCornerRadiusWhenRestored
                          : 0;

  PaintFrameImagesInRoundRect(canvas, frame_image, frame_overlay_image,
                              appearance_provider_->GetFrameHeaderColor(active),
                              GetPaintedBounds(), GetThemeBackgroundXInset(),
                              appearance_provider_->GetFrameHeaderImageYInset(),
                              alpha, corner_radius);
}
