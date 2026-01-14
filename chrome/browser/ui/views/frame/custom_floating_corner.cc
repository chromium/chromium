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
  }
}

}  // namespace

CustomFloatingCorner::CustomFloatingCorner(
    BrowserView& browser_view,
    CornerOrientation orientation,
    views::ShapeContextTokens corner_radius_token,
    ColorChoice color,
    std::optional<ui::ColorId> stroke_color)
    : CustomCorners(browser_view),
      orientation_(orientation),
      corner_radius_token_(corner_radius_token),
      color_(color),
      stroke_color_(stroke_color) {
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

void CustomFloatingCorner::SetStrokeColor(
    std::optional<ui::ColorId> stroke_color) {
  if (stroke_color_ == stroke_color) {
    return;
  }

  const bool size_changed =
      stroke_color.has_value() != stroke_color_.has_value();

  stroke_color_ = stroke_color;
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
  const float corner_size = corner_radius + views::Separator::kThickness;
  return gfx::Size(corner_size, corner_size);
}

void CustomFloatingCorner::OnPaint(gfx::Canvas* canvas) {
  const gfx::Rect rect(GetLocalBounds());

  gfx::ScopedCanvas scoped(canvas);

  // This assumes that the view has gotten its preferred size, however, it will
  // scale gracefully if it is not that size. The view should be the preferred
  // corner radius in each dimension, plus the stroke thickness if there is a
  // stroke.
  const bool has_stroke = stroke_color_.has_value();
  const SkVector corner_radius(
      has_stroke ? width() - views::Separator::kThickness : width(),
      has_stroke ? height() - views::Separator::kThickness : height());

  // Because we're painting, we have to account for RTL.
  const CornerOrientation visual_orientation =
      GetVisualOrientation(orientation_);

  // Set up a clip path.
  SkPathBuilder clip_path;
  switch (visual_orientation) {
    case CornerOrientation::kTopLeading:
      clip_path.moveTo(0, 0);
      clip_path.lineTo(rect.width(), 0);
      if (has_stroke) {
        clip_path.lineTo(rect.width(), views::Separator::kThickness);
      }
      clip_path.arcTo(corner_radius, 0, SkPathBuilder::kSmall_ArcSize,
                      SkPathDirection::kCCW,
                      SkPoint(has_stroke ? views::Separator::kThickness : 0,
                              rect.height()));
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
        clip_path.lineTo(rect.width() - views::Separator::kThickness,
                         rect.height());
      }
      clip_path.arcTo(
          corner_radius, 0, SkPathBuilder::kSmall_ArcSize,
          SkPathDirection::kCCW,
          SkPoint(0, has_stroke ? views::Separator::kThickness : 0));
      if (has_stroke) {
        clip_path.lineTo(0, 0);
      }
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
    flags.setStrokeWidth(views::Separator::kThickness * 2);
    flags.setColor(GetColorProvider()->GetColor(*stroke_color_));
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setAntiAlias(true);

    SkPathBuilder stroke_path;
    switch (visual_orientation) {
      case CornerOrientation::kTopLeading:
        stroke_path.moveTo(rect.width(), views::Separator::kThickness);
        stroke_path.arcTo(corner_radius, 0, SkPathBuilder::kSmall_ArcSize,
                          SkPathDirection::kCCW,
                          SkPoint(views::Separator::kThickness, rect.height()));
        break;
      case CornerOrientation::kTopTrailing:
        stroke_path.moveTo(rect.width() - views::Separator::kThickness,
                           rect.height());
        stroke_path.arcTo(corner_radius, 0, SkPathBuilder::kSmall_ArcSize,
                          SkPathDirection::kCCW,
                          SkPoint(0, views::Separator::kThickness));
        break;
    }
    canvas->DrawPath(stroke_path.detach(), flags);
  }
}

const views::View& CustomFloatingCorner::GetView() const {
  return *this;
}

void CustomFloatingCorner::OnBrowserPaintAsActiveChanged() {
  if (std::holds_alternative<FrameColor>(color_)) {
    SchedulePaint();
  }
}

BEGIN_METADATA(CustomFloatingCorner)
END_METADATA
