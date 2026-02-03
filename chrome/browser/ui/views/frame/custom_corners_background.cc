// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/custom_corners_background.h"

#include <variant>

#include "build/build_config.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/browser_widget.h"
#include "chrome/browser/ui/views/frame/top_container_background.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "ui/color/color_id.h"
#include "ui/color/color_variant.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect_f.h"
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

CustomCornersBackground::CustomCornersBackground(
    views::View& view,
    BrowserView& browser_view,
    ColorChoice primary_color,
    ColorChoice corner_color,
    std::optional<int> default_radius)
    : CustomCorners(browser_view),
      primary_color_(primary_color),
      corner_color_(corner_color),
      default_radius_(default_radius.value_or(
          GetLayoutConstant(LayoutConstant::kToolbarCornerRadius))),
      view_(view) {}

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

void CustomCornersBackground::SetOutline(const Outline& outline) {
  if (outline_ == outline) {
    return;
  }

  outline_ = outline;
  view_->SchedulePaint();
}

CustomCornersBackground::Corner CustomCornersBackground::GetWindowCorner(
    bool upper) const {
  Corner corner;
#if BUILDFLAG(IS_CHROMEOS)
  if (chromeos::features::IsRoundedWindowsEnabled()) {
    corner.type = CornerType::kRounded;
    // The corners should be symmetrical here; if they're not, revisit.
    corner.radius = chromeos::kRoundedWindowCornerRadius;
  } else {
    corner.type = CornerType::kSquare;
  }
#elif BUILDFLAG(IS_LINUX)
  if (auto* const widget = browser_view().browser_widget()) {
    if (auto* const frame = widget->GetFrameView()) {
      const auto rrect = frame->GetRestoredClipRegion();
      const auto radii = rrect.radii(upper ? SkRRect::kUpperLeft_Corner
                                           : SkRRect::kLowerLeft_Corner);
      corner.radius = base::ClampRound((radii.x() + radii.y()) / 2);
      corner.type =
          corner.radius > 0 ? CornerType::kRounded : CornerType::kSquare;
    }
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

  gfx::Rect rect(view->GetLocalBounds());

  // Draw corners behind where necessary using the background color.
  if (corners_.upper_leading.type == CornerType::kRoundedWithBackground) {
    const int corner_radius =
        corners_.upper_leading.radius.value_or(default_radius_);
    const SkPath corner_path =
        SkPath::Rect(SkRect::MakeXYWH(0, 0, corner_radius, corner_radius));
    PaintPath(canvas, corner_path, corner_color_, /*anti_alias=*/false);
  }
  if (corners_.upper_trailing.type == CornerType::kRoundedWithBackground) {
    const int corner_radius =
        corners_.upper_trailing.radius.value_or(default_radius_);
    const SkPath corner_path = SkPath::Rect(SkRect::MakeXYWH(
        rect.width() - corner_radius, 0, corner_radius, corner_radius));
    PaintPath(canvas, corner_path, corner_color_, /*anti_alias=*/false);
  }
  if (corners_.lower_trailing.type == CornerType::kRoundedWithBackground) {
    const int corner_radius =
        corners_.lower_trailing.radius.value_or(default_radius_);
    const SkPath corner_path = SkPath::Rect(SkRect::MakeXYWH(
        rect.width() - corner_radius, rect.height() - corner_radius,
        corner_radius, corner_radius));
    PaintPath(canvas, corner_path, corner_color_, /*anti_alias=*/false);
  }
  if (corners_.lower_leading.type == CornerType::kRoundedWithBackground) {
    const int corner_radius =
        corners_.lower_leading.radius.value_or(default_radius_);
    const SkPath corner_path = SkPath::Rect(SkRect::MakeXYWH(
        0, rect.height() - corner_radius, corner_radius, corner_radius));
    PaintPath(canvas, corner_path, corner_color_, /*anti_alias=*/false);
  }

  // Draw solid rect/rrect background:
  SkVector radii[4] = {
      CornerToRadiusVector(corners_.upper_leading, default_radius_),
      CornerToRadiusVector(corners_.upper_trailing, default_radius_),
      CornerToRadiusVector(corners_.lower_trailing, default_radius_),
      CornerToRadiusVector(corners_.lower_leading, default_radius_)};
  const SkPath path =
      SkPath::RRect(SkRRect::MakeRectRadii(gfx::RectToSkRect(rect), radii));
  PaintPath(canvas, path, primary_color_, /*anti_alias=*/true);

  // Paint strokes around the outside. Corners get strokes if they are between
  // two sides with strokes and have a radius. Multiple paths may be drawn if
  // the sides with outlines are disconnected.
  if (outline_.has_strokes()) {
    cc::PaintFlags stroke_flags;
    stroke_flags.setStrokeWidth(views::Separator::kThickness);
    stroke_flags.setColor(
        GetView().GetColorProvider()->GetColor(outline_.color));
    stroke_flags.setStyle(cc::PaintFlags::kStroke_Style);
    stroke_flags.setAntiAlias(true);

    // Shrink the bounds and radii by half the stroke width.
    const float kHalfStroke = views::Separator::kThickness * 0.5f;
    gfx::RectF stroke_bounds(rect);
    stroke_bounds.Inset(kHalfStroke);
    for (auto& radius : radii) {
      radius.set(std::max(radius.x() - kHalfStroke, 0.0f),
                 std::max(radius.y() - kHalfStroke, 0.0f));
    }

    SkPathBuilder stroke_path;

    // Start by drawing the top line.
    if (outline_.top) {
      stroke_path.moveTo(stroke_bounds.x() + radii[0].x(), stroke_bounds.y());
      stroke_path.lineTo(stroke_bounds.right() - radii[1].x(),
                         stroke_bounds.y());

      // Maybe draw the upper trailing corner as well.
      if (outline_.trailing && !radii[1].isZero()) {
        stroke_path.arcTo(
            radii[1], 0, SkPathBuilder::kSmall_ArcSize, SkPathDirection::kCW,
            SkPoint(stroke_bounds.right(), stroke_bounds.y() + radii[1].y()));
      }
    }

    // Next, draw the right side if present.
    if (outline_.trailing) {
      if (stroke_path.isEmpty()) {
        stroke_path.moveTo(stroke_bounds.right(),
                           stroke_bounds.y() + radii[1].y());
      }
      stroke_path.lineTo(stroke_bounds.right(),
                         stroke_bounds.bottom() - radii[2].y());

      // Maybe draw the bottom trailing corner.
      if (outline_.bottom && !radii[2].isZero()) {
        stroke_path.arcTo(radii[2], 0, SkPathBuilder::kSmall_ArcSize,
                          SkPathDirection::kCW,
                          SkPoint(stroke_bounds.right() - radii[2].x(),
                                  stroke_bounds.bottom()));
      }
    } else if (!stroke_path.isEmpty()) {
      // If there is no right side, perhaps complete the current stroke.
      canvas->DrawPath(stroke_path.detach(), stroke_flags);
    }

    // Next, draw the bottom if present.
    if (outline_.bottom) {
      if (stroke_path.isEmpty()) {
        stroke_path.moveTo(stroke_bounds.right() - radii[2].x(),
                           stroke_bounds.bottom());
      }
      stroke_path.lineTo(stroke_bounds.x() + radii[3].x(),
                         stroke_bounds.bottom());

      // Maybe draw the bottom leading corner.
      if (outline_.leading && !radii[3].isZero()) {
        stroke_path.arcTo(
            radii[3], 0, SkPathBuilder::kSmall_ArcSize, SkPathDirection::kCW,
            SkPoint(stroke_bounds.x(), stroke_bounds.bottom() - radii[3].y()));
      }
    } else if (!stroke_path.isEmpty()) {
      // If there is no bottom, perhaps complete the current stroke.
      canvas->DrawPath(stroke_path.detach(), stroke_flags);
    }

    // Next, draw the left side if present.
    if (outline_.leading) {
      if (stroke_path.isEmpty()) {
        stroke_path.moveTo(stroke_bounds.x(),
                           stroke_bounds.bottom() - radii[3].y());
      }

      // Maybe draw the top leading corner.
      stroke_path.lineTo(stroke_bounds.x(), stroke_bounds.y() + radii[0].y());
      if (outline_.top && !radii[0].isZero()) {
        stroke_path.arcTo(
            radii[0], 0, SkPathBuilder::kSmall_ArcSize, SkPathDirection::kCW,
            SkPoint(stroke_bounds.x() + radii[0].x(), stroke_bounds.y()));
      }
    }

    // Always draw the last stroke if one is present.
    if (!stroke_path.isEmpty()) {
      canvas->DrawPath(stroke_path.detach(), stroke_flags);
    }
  }
}

std::optional<gfx::RoundedCornersF>
CustomCornersBackground::GetRoundedCornerRadii() const {
  // Provided for completeness; this is not used anywhere.
  return gfx::RoundedCornersF(
      CornerToRadius(corners_.upper_leading, default_radius_),
      CornerToRadius(corners_.upper_trailing, default_radius_),
      CornerToRadius(corners_.lower_trailing, default_radius_),
      CornerToRadius(corners_.lower_leading, default_radius_));
}

const views::View& CustomCornersBackground::GetView() const {
  return *view_;
}

void CustomCornersBackground::OnBrowserPaintAsActiveChanged() {
  if (std::holds_alternative<FrameColor>(primary_color_) ||
      std::holds_alternative<FrameColor>(corner_color_)) {
    view_->SchedulePaint();
  }
}
