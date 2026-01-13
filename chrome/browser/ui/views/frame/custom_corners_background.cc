// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/custom_corners_background.h"

#include <variant>

#include "build/build_config.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_background.h"
#include "ui/color/color_id.h"
#include "ui/color/color_variant.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/ui/base/chromeos_ui_constants.h"
#endif

namespace {

int CornerToRadius(const CustomCornersBackground::Corner& corner,
                   int default_radius) {
  return corner.type == CustomCornersBackground::CornerType::kSquare
             ? 0
             : corner.radius.value_or(default_radius);
}

SkVector CornerToRadiusVector(const CustomCornersBackground::Corner& corner,
                              int default_radius) {
  const int radius = CornerToRadius(corner, default_radius);
  return SkVector(radius, radius);
}

}  // namespace

CustomCornersBackground::CustomCornersBackground(views::View& view,
                                                 BrowserView& browser_view,
                                                 ColorChoice primary_color,
                                                 ColorChoice corner_color)
    : primary_color_(primary_color),
      corner_color_(corner_color),
      view_(view),
      browser_view_(browser_view) {
  // Browser may not yet have a widget, but we need to track the widget in
  // case we need active/inactive frame color.
  if (browser_view_->GetWidget()) {
    // This hooks up the active/inactive state listener.
    OnViewAddedToWidget(&browser_view);
  } else {
    // This will hook up the listener when the browser view is added to a
    // widget.
    browser_view_observation_.Observe(&browser_view);
  }
}

CustomCornersBackground::~CustomCornersBackground() = default;

void CustomCornersBackground::SetVisible(bool visible) {
  if (visible_ == visible) {
    return;
  }

  visible_ = visible;
  view_->SchedulePaint();
}

void CustomCornersBackground::SetPrimaryColor(ColorChoice primary_color) {
  if (primary_color_ == primary_color) {
    return;
  }

  primary_color_ = primary_color;
  view_->SchedulePaint();
}

void CustomCornersBackground::SetCornerColor(ColorChoice corner_color) {
  if (corner_color == corner_color_) {
    return;
  }

  corner_color_ = corner_color;
  view_->SchedulePaint();
}

void CustomCornersBackground::SetCorners(const Corners& corners) {
  if (corners_ == corners) {
    return;
  }

  corners_ = corners;
  view_->SchedulePaint();
}

// static
CustomCornersBackground::Corner CustomCornersBackground::GetWindowCorner() {
  Corner corner;
#if BUILDFLAG(IS_CHROMEOS)
  if (chromeos::features::IsRoundedWindowsEnabled()) {
    corner.type = CornerType::kRounded;
    // The corners should be symmetrical here; if they're not, revisit.
    corner.radius = chromeos::kRoundedWindowCornerRadius;
  } else {
    corner.type = CornerType::kSquare;
  }
#else
  corner.type = CornerType::kRounded;
#endif
  return corner;
}

void CustomCornersBackground::Paint(gfx::Canvas* canvas,
                                    views::View* view) const {
  if (!visible_) {
    return;
  }

  if (view->layer()) {
    CHECK(!view->layer()->fills_bounds_opaquely());
  }

  const gfx::Rect rect(view->GetLocalBounds());
  const int default_corner_radius =
      GetLayoutConstant(LayoutConstant::kToolbarCornerRadius);

  // Draw corners behind where necessary using the background color.
  if (corners_.upper_leading.type == CornerType::kRoundedWithBackground) {
    const int corner_radius =
        corners_.upper_leading.radius.value_or(default_corner_radius);
    const SkPath corner_path =
        SkPath::Rect(SkRect::MakeXYWH(0, 0, corner_radius, corner_radius));
    PaintPath(canvas, corner_path, corner_color_, /*anti_alias=*/false);
  }
  if (corners_.upper_trailing.type == CornerType::kRoundedWithBackground) {
    const int corner_radius =
        corners_.upper_trailing.radius.value_or(default_corner_radius);
    const SkPath corner_path = SkPath::Rect(SkRect::MakeXYWH(
        rect.width() - corner_radius, 0, corner_radius, corner_radius));
    PaintPath(canvas, corner_path, corner_color_, /*anti_alias=*/false);
  }
  if (corners_.lower_trailing.type == CornerType::kRoundedWithBackground) {
    const int corner_radius =
        corners_.lower_trailing.radius.value_or(default_corner_radius);
    const SkPath corner_path = SkPath::Rect(SkRect::MakeXYWH(
        rect.width() - corner_radius, rect.height() - corner_radius,
        corner_radius, corner_radius));
    PaintPath(canvas, corner_path, corner_color_, /*anti_alias=*/false);
  }
  if (corners_.lower_leading.type == CornerType::kRoundedWithBackground) {
    const int corner_radius =
        corners_.lower_leading.radius.value_or(default_corner_radius);
    const SkPath corner_path = SkPath::Rect(SkRect::MakeXYWH(
        0, rect.height() - corner_radius, corner_radius, corner_radius));
    PaintPath(canvas, corner_path, corner_color_, /*anti_alias=*/false);
  }

  // Draw solid rect/rrect background:
  const SkVector radii[4] = {
      CornerToRadiusVector(corners_.upper_leading, default_corner_radius),
      CornerToRadiusVector(corners_.upper_trailing, default_corner_radius),
      CornerToRadiusVector(corners_.lower_trailing, default_corner_radius),
      CornerToRadiusVector(corners_.lower_leading, default_corner_radius)};
  const SkPath path =
      SkPath::RRect(SkRRect::MakeRectRadii(gfx::RectToSkRect(rect), radii));
  PaintPath(canvas, path, primary_color_, /*anti_alias=*/true);
}

std::optional<gfx::RoundedCornersF>
CustomCornersBackground::GetRoundedCornerRadii() const {
  // Provided for completeness; this is not used anywhere.
  const int default_corner_radius =
      GetLayoutConstant(LayoutConstant::kToolbarCornerRadius);
  return gfx::RoundedCornersF(
      CornerToRadius(corners_.upper_leading, default_corner_radius),
      CornerToRadius(corners_.upper_trailing, default_corner_radius),
      CornerToRadius(corners_.lower_trailing, default_corner_radius),
      CornerToRadius(corners_.lower_leading, default_corner_radius));
}

void CustomCornersBackground::PaintPath(gfx::Canvas* canvas,
                                        const SkPath& path,
                                        ColorChoice color_choice,
                                        bool anti_alias) const {
  if (std::holds_alternative<TopContainerTheme>(color_choice)) {
    gfx::ScopedCanvas scoped(canvas);
    canvas->ClipPath(path, anti_alias);
    TopContainerBackground::PaintBackground(canvas, &*view_, &*browser_view_);
    return;
  }

  ui::ColorVariant color;
  if (std::holds_alternative<FrameColor>(color_choice)) {
    color = browser_view_->GetWidget()->ShouldPaintAsActive()
                ? ui::kColorFrameActive
                : ui::kColorFrameInactive;
  } else {
    color = std::get<ui::ColorId>(color_choice);
  }

  cc::PaintFlags flags;
  flags.setAntiAlias(anti_alias);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(color.ResolveToSkColor(view_->GetColorProvider()));
  canvas->DrawPath(path, flags);
}

void CustomCornersBackground::OnViewAddedToWidget(views::View* view) {
  CHECK_EQ(view, &*browser_view_);
  browser_view_observation_.Reset();
  browser_frame_active_subscription_ =
      view->GetWidget()->RegisterPaintAsActiveChangedCallback(
          base::BindRepeating(
              &CustomCornersBackground::OnBrowserPaintAsActiveChanged,
              base::Unretained(this)));
}

void CustomCornersBackground::OnBrowserPaintAsActiveChanged() {
  if (std::holds_alternative<FrameColor>(primary_color_) ||
      std::holds_alternative<FrameColor>(corner_color_)) {
    view_->SchedulePaint();
  }
}
