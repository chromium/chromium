// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/custom_floating_corner.h"

#include <optional>
#include <variant>

#include "base/i18n/rtl.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/layout_provider.h"

namespace {

// Returns the orientation in which to visually draw the corner; will mirror
// `orientation` for RtL.
CustomFloatingCorner::CornerOrientation GetVisualOrientation(
    CustomFloatingCorner::CornerOrientation orientation) {
  if (!base::i18n::IsRTL()) {
    return orientation;
  }

  switch (orientation) {
    case CustomFloatingCorner::CornerOrientation::kTopLeading:
      return CustomFloatingCorner::CornerOrientation::kTopTrailing;
    case CustomFloatingCorner::CornerOrientation::kTopTrailing:
      return CustomFloatingCorner::CornerOrientation::kTopLeading;
    case CustomFloatingCorner::CornerOrientation::kBottomLeading:
      return CustomFloatingCorner::CornerOrientation::kBottomTrailing;
    case CustomFloatingCorner::CornerOrientation::kBottomTrailing:
      return CustomFloatingCorner::CornerOrientation::kBottomLeading;
  }
}

}  // namespace

CustomFloatingCorner::CustomFloatingCorner(
    BrowserView& browser_view,
    CornerOrientation orientation,
    views::ShapeContextTokens corner_radius_token,
    ColorChoice color,
    std::optional<ui::ColorId> stroke_color,
    bool is_vertical_window_edge)
    : CustomCorners(browser_view),
      orientation_(orientation),
      corner_radius_token_(corner_radius_token),
      color_(color),
      stroke_color_(stroke_color),
      is_vertical_window_edge_(is_vertical_window_edge) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

CustomFloatingCorner::~CustomFloatingCorner() = default;

void CustomFloatingCorner::SetColor(ColorChoice color) {
  if (color_ == color) {
    return;
  }

  color_ = color;
  SchedulePaint();
}

void CustomFloatingCorner::SetOrientation(CornerOrientation orientation) {
  if (orientation_ == orientation) {
    return;
  }

  orientation_ = orientation;
  SchedulePaint();
}

void CustomFloatingCorner::SetCornerRadius(
    views::ShapeContextTokens corner_radius_token) {
  if (corner_radius_token == corner_radius_token_) {
    return;
  }

  corner_radius_token_ = corner_radius_token;
  PreferredSizeChanged();
}

void CustomFloatingCorner::SetStroke(std::optional<ui::ColorId> stroke_color,
                                     bool is_vertical_window_edge) {
  if (stroke_color_ == stroke_color &&
      is_vertical_window_edge_ == is_vertical_window_edge) {
    return;
  }

  const bool size_changed =
      stroke_color.has_value() != stroke_color_.has_value() ||
      is_vertical_window_edge != is_vertical_window_edge_;

  stroke_color_ = stroke_color;
  is_vertical_window_edge_ = is_vertical_window_edge;
  if (size_changed) {
    PreferredSizeChanged();
  } else {
    SchedulePaint();
  }
}

gfx::Size CustomFloatingCorner::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  // This can return nullptr when there is no Widget (for context, see
  // http://crbug.com/40178332). The nullptr dereference does not always
  // crash due to compiler optimizations, so CHECKing here ensures we crash.
  CHECK(GetLayoutProvider());
  const float corner_radius =
      GetLayoutProvider()->GetCornerRadiusMetric(corner_radius_token_);
  const float horizontal_size =
      corner_radius + (stroke_color_ ? views::Separator::kThickness : 0);
  const float vertical_size =
      corner_radius + (stroke_color_ && !is_vertical_window_edge_
                           ? views::Separator::kThickness
                           : 0);
  return gfx::Size(horizontal_size, vertical_size);
}

void CustomFloatingCorner::OnPaint(gfx::Canvas* canvas) {
  const int kStrokeSize = views::Separator::kThickness;
  const gfx::Rect rect(GetLocalBounds());

  gfx::ScopedCanvas scoped(canvas);

  // This assumes that the view has gotten its preferred size, however, it will
  // scale gracefully if it is not that size. The view should be the preferred
  // corner radius in each dimension, plus the stroke thickness if there is a
  // stroke.
  const bool has_stroke = stroke_color_.has_value();
  const bool extend_vertical = has_stroke && !is_vertical_window_edge_;
  const SkVector corner_radius(
      has_stroke ? width() - kStrokeSize : width(),
      extend_vertical ? height() - kStrokeSize : height());

  // Because we're painting, we have to account for RTL.
  const CornerOrientation visual_orientation =
      GetVisualOrientation(orientation_);

  // Set up a clip path.
  SkPathBuilder clip_path;
  switch (visual_orientation) {
    case CornerOrientation::kTopLeading:
      clip_path.moveTo(0, 0);
      clip_path.lineTo(rect.width(), 0);
      if (extend_vertical) {
        clip_path.lineTo(rect.width(), kStrokeSize);
      }
      clip_path.arcTo(corner_radius, 0, SkPathBuilder::kSmall_ArcSize,
                      SkPathDirection::kCCW,
                      SkPoint(has_stroke ? kStrokeSize : 0, rect.height()));
      if (has_stroke) {
        clip_path.lineTo(0, rect.height());
      }
      clip_path.lineTo(0, 0);
      break;
    case CornerOrientation::kTopTrailing:
      clip_path.moveTo(0, 0);
      clip_path.lineTo(rect.width(), 0);
      clip_path.lineTo(rect.width(), rect.height());
      if (has_stroke) {
        clip_path.lineTo(rect.width() - kStrokeSize, rect.height());
      }
      clip_path.arcTo(corner_radius, 0, SkPathBuilder::kSmall_ArcSize,
                      SkPathDirection::kCCW,
                      SkPoint(0, extend_vertical ? kStrokeSize : 0));
      if (extend_vertical) {
        clip_path.lineTo(0, 0);
      }
      break;
    case CornerOrientation::kBottomLeading:
      clip_path.moveTo(0, 0);
      if (has_stroke) {
        clip_path.lineTo(kStrokeSize, 0);
      }
      clip_path.arcTo(
          corner_radius, 0, SkPathBuilder::kSmall_ArcSize,
          SkPathDirection::kCCW,
          SkPoint(rect.width(), extend_vertical ? rect.height() - kStrokeSize
                                                : rect.height()));
      if (extend_vertical) {
        clip_path.lineTo(rect.width(), rect.height());
      }
      clip_path.lineTo(0, rect.height());
      clip_path.lineTo(0, 0);
      break;
    case CornerOrientation::kBottomTrailing:
      clip_path.moveTo(rect.width(), 0);
      clip_path.lineTo(rect.width(), rect.height());
      clip_path.lineTo(0, rect.height());
      if (extend_vertical) {
        clip_path.lineTo(0, rect.height() - kStrokeSize);
      }
      clip_path.arcTo(
          corner_radius, 0, SkPathBuilder::kSmall_ArcSize,
          SkPathDirection::kCCW,
          SkPoint(has_stroke ? rect.width() - kStrokeSize : rect.width(), 0));
      break;
  }
  canvas->ClipPath(clip_path.detach(), /*do_anti_alias=*/true);

  // Fill the clipped canvas.
  PaintPath(canvas,
            SkPath::Rect(SkRect::MakeXYWH(rect.x(), rect.y(), rect.width(),
                                          rect.height())),
            color_, false);

  // Maybe draw the stroke.
  if (has_stroke) {
    cc::PaintFlags flags;
    flags.setStrokeWidth(kStrokeSize * 2);
    flags.setColor(GetColorProvider()->GetColor(*stroke_color_));
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setAntiAlias(true);

    SkPathBuilder stroke_path;
    switch (visual_orientation) {
      case CornerOrientation::kTopLeading:
        stroke_path.moveTo(rect.width(), extend_vertical ? kStrokeSize : 0);
        stroke_path.arcTo(corner_radius, 0, SkPathBuilder::kSmall_ArcSize,
                          SkPathDirection::kCCW,
                          SkPoint(kStrokeSize, rect.height()));
        break;
      case CornerOrientation::kTopTrailing:
        stroke_path.moveTo(rect.width() - kStrokeSize, rect.height());
        stroke_path.arcTo(corner_radius, 0, SkPathBuilder::kSmall_ArcSize,
                          SkPathDirection::kCCW,
                          SkPoint(0, extend_vertical ? kStrokeSize : 0));
        break;
      case CornerOrientation::kBottomLeading:
        stroke_path.moveTo(kStrokeSize, 0);
        stroke_path.arcTo(
            corner_radius, 0, SkPathBuilder::kSmall_ArcSize,
            SkPathDirection::kCCW,
            SkPoint(rect.width(), extend_vertical ? rect.height() - kStrokeSize
                                                  : rect.height()));
        break;
      case CornerOrientation::kBottomTrailing:
        stroke_path.moveTo(rect.width() - kStrokeSize, 0);
        stroke_path.arcTo(
            corner_radius, 0, SkPathBuilder::kSmall_ArcSize,
            SkPathDirection::kCW,
            SkPoint(0, extend_vertical ? rect.height() - kStrokeSize
                                       : rect.height()));
        break;
    }
    canvas->DrawPath(stroke_path.detach(), flags);
  }
}

void CustomFloatingCorner::OnThemeChanged() {
  views::View::OnThemeChanged();
  SchedulePaint();
}

const views::View& CustomFloatingCorner::GetView() const {
  return *this;
}

void CustomFloatingCorner::OnBrowserPaintAsActiveChanged() {
  if (std::holds_alternative<FrameTheme>(color_)) {
    SchedulePaint();
  }
}

void CustomFloatingCorner::SchedulePaintHost() {
  SchedulePaint();
}

BEGIN_METADATA(CustomFloatingCorner)
END_METADATA
