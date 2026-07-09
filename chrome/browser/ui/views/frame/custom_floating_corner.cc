// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/custom_floating_corner.h"

#include <optional>
#include <variant>

#include "base/i18n/rtl.h"
#include "chrome/browser/ui/color/chrome_color_provider_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/layout_provider.h"

namespace {

constexpr int kStrokeSize = views::Separator::kThickness;

}  // namespace

CustomFloatingCorner::CustomFloatingCorner(
    BrowserView& browser_view,
    CornerOrientation orientation,
    views::ShapeContextTokens corner_radius_token,
    ColorChoiceWithAlpha background,
    std::optional<ColorWithAlpha> stroke_color,
    bool is_vertical_window_edge)
    : CustomCorners(browser_view),
      orientation_(orientation),
      corner_radius_token_(corner_radius_token),
      background_(background),
      stroke_(stroke_color),
      is_vertical_window_edge_(is_vertical_window_edge) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

CustomFloatingCorner::CustomFloatingCorner(
    BrowserView& browser_view,
    CornerOrientation orientation,
    views::ShapeContextTokens corner_radius_token,
    ColorChoice background,
    std::optional<ui::ColorId> stroke,
    bool is_vertical_window_edge)
    : CustomCorners(browser_view),
      orientation_(orientation),
      corner_radius_token_(corner_radius_token),
      background_(background),
      stroke_(stroke),
      is_vertical_window_edge_(is_vertical_window_edge) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

CustomFloatingCorner::~CustomFloatingCorner() = default;

void CustomFloatingCorner::SetBackground(ColorChoiceWithAlpha color) {
  if (background_ == color) {
    return;
  }

  background_ = color;
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

void CustomFloatingCorner::SetStroke(std::optional<ColorWithAlpha> stroke_color,
                                     bool is_vertical_window_edge) {
  if (stroke_ == stroke_color &&
      is_vertical_window_edge_ == is_vertical_window_edge) {
    return;
  }

  // The size changes if the stroke changes because the stroke width is added to
  // the preferred size.
  const bool size_changed = stroke_color.has_value() != stroke_.has_value() ||
                            is_vertical_window_edge != is_vertical_window_edge_;

  stroke_ = stroke_color;
  is_vertical_window_edge_ = is_vertical_window_edge;
  if (size_changed) {
    PreferredSizeChanged();
  } else {
    SchedulePaint();
  }
}

void CustomFloatingCorner::SetAlpha(float alpha) {
  if (stroke_) {
    auto new_stroke = *stroke_;
    new_stroke.opacity = alpha;
    SetStroke(new_stroke, is_vertical_window_edge_);
  }
  auto background = background_;
  background.opacity = alpha;
  SetBackground(background);
}

SkPath CustomFloatingCorner::GetBackgroundPath(
    const gfx::Rect& in_bounds) const {
  // This assumes that the view has gotten its preferred size, however, it will
  // scale gracefully if it is not that size. The view should be the preferred
  // corner radius in each dimension, plus the stroke thickness if there is a
  // stroke.
  const bool has_stroke = stroke_.has_value();
  const bool extend_vertical = has_stroke && !is_vertical_window_edge_;
  const int horizontal = has_stroke ? kStrokeSize : 0;
  const int vertical = extend_vertical ? kStrokeSize : 0;
  gfx::Insets stroke_insets;

  // Because we're painting, we have to account for RTL.
  const auto visual_corner = ToVisualCorner(orientation_);

  switch (visual_corner) {
    case VisualCorner::kTopLeft:
      stroke_insets = gfx::Insets::TLBR(vertical, horizontal, 0, 0);
      break;
    case VisualCorner::kTopRight:
      stroke_insets = gfx::Insets::TLBR(vertical, 0, 0, horizontal);
      break;
    case VisualCorner::kBottomRight:
      stroke_insets = gfx::Insets::TLBR(0, 0, vertical, horizontal);
      break;
    case VisualCorner::kBottomLeft:
      stroke_insets = gfx::Insets::TLBR(0, horizontal, vertical, 0);
      break;
  }

  return GetCornerPath(visual_corner, in_bounds, stroke_insets);
}

int CustomFloatingCorner::GetCornerRadius() const {
  return GetLayoutProvider()->GetCornerRadiusMetric(corner_radius_token_);
}

gfx::Size CustomFloatingCorner::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  // This can return nullptr when there is no Widget (for context, see
  // http://crbug.com/40178332). The nullptr dereference does not always
  // crash due to compiler optimizations, so CHECKing here ensures we crash.
  CHECK(GetLayoutProvider());
  const float corner_radius = GetCornerRadius();
  const float horizontal_size =
      corner_radius + (stroke_ ? views::Separator::kThickness : 0);
  const float vertical_size =
      corner_radius +
      (stroke_ && !is_vertical_window_edge_ ? views::Separator::kThickness : 0);
  return gfx::Size(horizontal_size, vertical_size);
}

void CustomFloatingCorner::OnPaint(gfx::Canvas* canvas) {
  const gfx::Rect rect(GetLocalBounds());

  gfx::ScopedCanvas scoped(canvas);

  // This assumes that the view has gotten its preferred size, however, it will
  // scale gracefully if it is not that size. The view should be the preferred
  // corner radius in each dimension, plus the stroke thickness if there is a
  // stroke.
  const bool has_stroke = stroke_ && stroke_->is_visible();
  const bool extend_vertical = has_stroke && !is_vertical_window_edge_;
  const SkVector corner_radius(
      has_stroke ? width() - kStrokeSize : width(),
      extend_vertical ? height() - kStrokeSize : height());

  canvas->ClipPath(GetBackgroundPath(rect), /*do_anti_alias=*/true);

  // Fill the clipped canvas.
  PaintPath(canvas,
            SkPath::Rect(SkRect::MakeXYWH(rect.x(), rect.y(), rect.width(),
                                          rect.height())),
            ColorChoiceWithAlpha(background_), false);

  // Maybe draw the stroke.
  if (has_stroke) {
    cc::PaintFlags flags;
    flags.setStrokeWidth(kStrokeSize * 2);
    SkColor stroke_color = GetColorProvider()->GetColor(stroke_->color);
    stroke_color = SkColorSetA(
        stroke_color,
        base::ClampRound(SkColorGetA(stroke_color) * stroke_->opacity));
    flags.setColor(stroke_color);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setAntiAlias(true);

    SkPathBuilder stroke_path;
    switch (ToVisualCorner(orientation_)) {
      case VisualCorner::kTopLeft:
        stroke_path.moveTo(rect.width(), extend_vertical ? kStrokeSize : 0);
        stroke_path.arcTo(corner_radius, 0, SkPathBuilder::kSmall_ArcSize,
                          SkPathDirection::kCCW,
                          SkPoint(kStrokeSize, rect.height()));
        break;
      case VisualCorner::kTopRight:
        stroke_path.moveTo(rect.width() - kStrokeSize, rect.height());
        stroke_path.arcTo(corner_radius, 0, SkPathBuilder::kSmall_ArcSize,
                          SkPathDirection::kCCW,
                          SkPoint(0, extend_vertical ? kStrokeSize : 0));
        break;
      case VisualCorner::kBottomLeft:
        stroke_path.moveTo(kStrokeSize, 0);
        stroke_path.arcTo(
            corner_radius, 0, SkPathBuilder::kSmall_ArcSize,
            SkPathDirection::kCCW,
            SkPoint(rect.width(), extend_vertical ? rect.height() - kStrokeSize
                                                  : rect.height()));
        break;
      case VisualCorner::kBottomRight:
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
  if (std::holds_alternative<FrameTheme>(background_.color)) {
    SchedulePaint();
  }
}

void CustomFloatingCorner::SchedulePaintHost() {
  SchedulePaint();
}

BEGIN_METADATA(CustomFloatingCorner)
END_METADATA
