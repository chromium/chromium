// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/default_frame_header.h"

#include "base/check.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/chromeos_ui_constants.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/frame/caption_buttons/frame_caption_button_container_view.h"
#include "chromeos/ui/wm/window_util.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/caption_button_layout_constants.h"

using views::Widget;

namespace {

// Tiles an image into an area, rounding the top corners.
void TileRoundRect(gfx::Canvas* canvas,
                   const cc::PaintFlags& flags,
                   const gfx::Rect& bounds,
                   int corner_radius) {
  SkRect rect = gfx::RectToSkRect(bounds);
  const SkScalar corner_radius_scalar = SkIntToScalar(corner_radius);
  SkScalar radii[8] = {corner_radius_scalar,
                       corner_radius_scalar,  // top-left
                       corner_radius_scalar,
                       corner_radius_scalar,  // top-right
                       0,
                       0,  // bottom-right
                       0,
                       0};  // bottom-left
  // Antialiasing can result in blending a transparent pixel and
  // leave non opaque alpha between the frame and the client area.
  // Extend 1dp to make sure it's fully opaque.
  rect.fBottom += 1;
  SkPath path;
  path.addRoundRect(rect, radii, SkPathDirection::kCW);
  canvas->DrawPath(path, flags);
}

// For now, we should only apply dynamic color to the default frame header if
// the window is a system web app.
bool ShouldApplyDynamicColor(aura::Window* window) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return window->GetProperty(chromeos::kAppTypeKey) ==
         chromeos::AppType::SYSTEM_APP;
#else
  // Default frame is used for non-browser frames in Lacros. In Lacros, we
  // never need dynamic colors as we don't display SWAs. This will need to be
  // redesigned if SWAs run in Lacros.
  return false;
#endif
}

}  // namespace

namespace chromeos {

///////////////////////////////////////////////////////////////////////////////
// DefaultFrameHeader, public:

DefaultFrameHeader::DefaultFrameHeader(
    views::Widget* target_widget,
    views::View* header_view,
    chromeos::FrameCaptionButtonContainerView* caption_button_container)
    : FrameHeader(target_widget, header_view) {
  DCHECK(caption_button_container);
  SetCaptionButtonContainer(caption_button_container);
  InitializeFrameColorMetricsHelper();
}

DefaultFrameHeader::~DefaultFrameHeader() = default;

void DefaultFrameHeader::SetWidthInPixels(int width_in_pixels) {
  if (width_in_pixels_ == width_in_pixels)
    return;
  width_in_pixels_ = width_in_pixels;
  SchedulePaintForTitle();
}

void DefaultFrameHeader::UpdateFrameColors() {
  aura::Window* target_window = GetTargetWindow();
  if (!target_window) {
    // b/302708285: This codepath is run during Widget teardown. In that
    // situation, `target_window` might be null and we won't display the
    // updated colors anyway.
    return;
  }

  const SkColor active_frame_color =
      target_window->GetProperty(kFrameActiveColorKey);
  const SkColor inactive_frame_color =
      target_window->GetProperty(kFrameInactiveColorKey);

  bool updated = false;
  // Update the frame if the frame color for the current active state changes.
  if (active_frame_color_ != active_frame_color) {
    active_frame_color_ = active_frame_color;
    updated = mode() == Mode::MODE_ACTIVE;
  }
  if (inactive_frame_color_ != inactive_frame_color) {
    inactive_frame_color_ = inactive_frame_color;
    updated |= mode() == Mode::MODE_INACTIVE;
  }

  if (updated) {
    StartTransitionAnimation(kDefaultFrameColorChangeAnimationDuration);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    frame_color_metrics_helper_->UpdateFrameColorChangesCount();
#endif
  }

  if (ShouldApplyDynamicColor(GetTargetWindow())) {
    UpdateCaptionButtonColors(mode() == MODE_ACTIVE
                                  ? ui::kColorSysPrimary
                                  : ui::kColorFrameCaptionButtonUnfocused);
  } else {
    UpdateCaptionButtonColors(std::nullopt);
  }
}

///////////////////////////////////////////////////////////////////////////////
// DefaultFrameHeader, protected:

void DefaultFrameHeader::DoPaintHeader(gfx::Canvas* canvas) {
  cc::PaintFlags flags;

  if (ShouldApplyDynamicColor(GetTargetWindow())) {
    flags.setColor(target_widget()->GetColorProvider()->GetColor(
        GetColorIdForCurrentMode()));
  } else {
    flags.setColor(mode() == Mode::MODE_ACTIVE ? active_frame_color_
                                               : inactive_frame_color_);
  }

  const int corner_radius = header_corner_radius();
  flags.setAntiAlias(corner_radius > 0);
  if (width_in_pixels_ > 0) {
    canvas->Save();
    float layer_scale =
        target_widget()->GetNativeWindow()->layer()->device_scale_factor();
    float canvas_scale = canvas->UndoDeviceScaleFactor();
    gfx::Rect rect =
        ScaleToEnclosingRect(GetPaintedBounds(), canvas_scale, canvas_scale);
    rect.set_width(width_in_pixels_ * canvas_scale / layer_scale);
    TileRoundRect(canvas, flags, rect,
                  static_cast<int>(corner_radius * canvas_scale));
    canvas->Restore();
  } else {
    TileRoundRect(canvas, flags, GetPaintedBounds(), corner_radius);
  }
  PaintTitleBar(canvas);
}

views::CaptionButtonLayoutSize DefaultFrameHeader::GetButtonLayoutSize() const {
  return views::CaptionButtonLayoutSize::kNonBrowserCaption;
}

SkColor DefaultFrameHeader::GetTitleColor() const {
  // Use IsDark() to change target colors instead of PickContrastingColor(), so
  // that FrameCaptionButton::GetButtonColor() (which uses different target
  // colors) can change between light/dark targets at the same time.  It looks
  // bad when the title and caption buttons disagree about whether to be light
  // or dark.
  const SkColor frame_color = GetCurrentFrameColor();
  const SkColor desired_color = color_utils::IsDark(frame_color)
                                    ? SK_ColorWHITE
                                    : SkColorSetRGB(40, 40, 40);
  return color_utils::BlendForMinContrast(desired_color, frame_color).color;
}

///////////////////////////////////////////////////////////////////////////////
// DefaultFrameHeader, private:

aura::Window* DefaultFrameHeader::GetTargetWindow() {
  return target_widget()->GetNativeWindow();
}

SkColor DefaultFrameHeader::GetCurrentFrameColor() const {
  return mode() == MODE_ACTIVE ? active_frame_color_ : inactive_frame_color_;
}

void DefaultFrameHeader::InitializeFrameColorMetricsHelper() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  aura::Window* window = GetTargetWindow();
  CHECK(window);
  frame_color_metrics_helper_ = std::make_unique<FrameColorMetricsHelper>(
      window->GetProperty(chromeos::kAppTypeKey));
#endif
}

}  // namespace chromeos
