// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/custom_corners_background.h"

#include <memory>
#include <variant>

#include "base/i18n/rtl.h"
#include "build/build_config.h"
#include "chrome/browser/ui/color/chrome_color_provider_utils.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/browser_widget.h"
#include "chrome/browser/ui/views/frame/custom_corners.h"
#include "chrome/browser/ui/views/frame/custom_floating_corner.h"
#include "chrome/browser/ui/views/frame/themed_background.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_variant.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

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

DEFINE_SAFE_CAST_TARGET(CustomCornersBackground)

CustomCornersBackground::CustomCornersBackground(
    views::View& view,
    BrowserView& browser_view,
    ColorChoiceWithAlpha primary_color,
    ColorChoiceWithAlpha corner_color,
    std::optional<int> default_radius)
    : CustomCorners(browser_view),
      primary_color_(primary_color),
      corner_color_(corner_color),
      default_radius_(default_radius.value_or(
          GetLayoutConstant(LayoutConstant::kToolbarCornerRadius))),
      view_(view) {}

CustomCornersBackground::CustomCornersBackground(
    views::View& view,
    BrowserView& browser_view,
    ColorChoice primary_color,
    ColorChoice corner_color,
    std::optional<int> default_radius)
    : CustomCornersBackground(view,
                              browser_view,
                              ColorChoiceWithAlpha(primary_color),
                              ColorChoiceWithAlpha(corner_color),
                              default_radius) {}

CustomCornersBackground::~CustomCornersBackground() = default;

void CustomCornersBackground::SetVisible(bool visible) {
  if (visible_ == visible) {
    return;
  }

  visible_ = visible;
  view_->SchedulePaint();
}

void CustomCornersBackground::SetPrimaryColor(
    ColorChoiceWithAlpha primary_color) {
  if (primary_color_ == primary_color) {
    return;
  }

  primary_color_ = primary_color;
  view_->SchedulePaint();
}

void CustomCornersBackground::SetCornerColor(
    ColorChoiceWithAlpha corner_color) {
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
  if (auto* const widget = browser_view().browser_widget()) {
    if (auto* const frame = widget->GetFrameView()) {
      const auto corners = frame->GetWindowRoundedCorners();
      corner.radius = upper ? corners.upper_left() : corners.lower_left();
      corner.type =
          corner.radius > 0 ? CornerType::kRounded : CornerType::kSquare;
    }
  }
  return corner;
}

void CustomCornersBackground::SetUseBackgroundBlur(bool use_background_blur) {
  static const bool background_blur_enabled =
      features::kGlassExpandOnHoverOpacity.Get() < 1.0f;
  static const float background_blur_radius =
      static_cast<float>(features::kGlassExpandOnHoverBlurRadius.Get());
  use_background_blur &=
      background_blur_enabled && background_blur_radius > 0.0f;
  if (!view_->layer()) {
    CHECK(!use_background_blur);
    return;
  }
  auto* const layer = view_->layer();
  CHECK(!layer->fills_bounds_opaquely());
  if (!use_background_blur) {
    layer->SetBackgroundBlur(0.0f);
    return;
  }
  layer->SetBackgroundBlur(background_blur_radius);
  layer->SetBackdropFilterBounds(GetBackgroundPath(view_->GetLocalBounds()));
  SchedulePaintHost();
}

SkPath CustomCornersBackground::GetBackgroundPath(const gfx::Rect& in_bounds,
                                                  CornerRadii* radii) const {
  const VisualCorners corners = GetMirroredCorners();
  CornerRadii local_radii;
  if (!radii) {
    radii = &local_radii;
  }
  *radii = {
      CornerToRadiusVector(corners[VisualCorner::kTopLeft], default_radius_),
      CornerToRadiusVector(corners[VisualCorner::kTopRight], default_radius_),
      CornerToRadiusVector(corners[VisualCorner::kBottomRight],
                           default_radius_),
      CornerToRadiusVector(corners[VisualCorner::kBottomLeft],
                           default_radius_)};
  return SkPath::RRect(
      SkRRect::MakeRectRadii(gfx::RectToSkRect(in_bounds), radii->data()));
}

std::vector<SkPath> CustomCornersBackground::GetCornerPaths(
    const gfx::Rect& in_bounds) const {
  std::vector<SkPath> result;
  const VisualCorners corners = GetMirroredCorners();

  // Bump out the corners by 1 DIP to avoid cracking/subpixel issues.
  // (This is why the insets are applied below.)
  if (corners[VisualCorner::kTopLeft].type != CornerType::kSquare) {
    const int radius =
        corners[VisualCorner::kTopLeft].radius.value_or(default_radius());
    gfx::Rect bounds(in_bounds.x(), in_bounds.y(), radius, radius);
    const gfx::Insets insets = gfx::Insets::TLBR(1, 1, 0, 0);
    bounds.Inset(-insets);
    result.push_back(GetCornerPath(VisualCorner::kTopLeft, bounds, insets));
  }
  if (corners[VisualCorner::kTopRight].type != CornerType::kSquare) {
    const int radius =
        corners[VisualCorner::kTopRight].radius.value_or(default_radius());
    gfx::Rect bounds(in_bounds.right() - radius, in_bounds.y(), radius, radius);
    const gfx::Insets insets = gfx::Insets::TLBR(1, 0, 0, 1);
    bounds.Inset(-insets);
    result.push_back(GetCornerPath(VisualCorner::kTopRight, bounds, insets));
  }
  if (corners[VisualCorner::kBottomRight].type != CornerType::kSquare) {
    const int radius =
        corners[VisualCorner::kBottomRight].radius.value_or(default_radius());
    gfx::Rect bounds(in_bounds.right() - radius, in_bounds.bottom() - radius,
                     radius, radius);
    const gfx::Insets insets = gfx::Insets::TLBR(0, 0, 1, 1);
    bounds.Inset(-insets);
    result.push_back(GetCornerPath(VisualCorner::kBottomRight, bounds, insets));
  }
  if (corners[VisualCorner::kBottomLeft].type != CornerType::kSquare) {
    const int radius =
        corners[VisualCorner::kBottomLeft].radius.value_or(default_radius());
    gfx::Rect bounds(in_bounds.x(), in_bounds.bottom() - radius, radius,
                     radius);
    const gfx::Insets insets = gfx::Insets::TLBR(0, 1, 1, 0);
    bounds.Inset(-insets);
    result.push_back(GetCornerPath(VisualCorner::kBottomLeft, bounds, insets));
  }
  return result;
}

void CustomCornersBackground::SetCutoutFrom(const Cutouts& cutouts) {
  std::vector<SkPath> new_cutout_paths;

  for (const auto& cutout : cutouts) {
    if (const views::View* const* view_ptr =
            std::get_if<const views::View*>(&cutout)) {
      const views::View* const view = *view_ptr;
      if (!view->GetVisible()) {
        continue;
      }
      const gfx::Rect bounds = views::View::ConvertRectFromScreen(
          &*view_, view->GetBoundsInScreen());
      SkPath cutout_path;
      if (auto* const corner = views::AsViewClass<CustomFloatingCorner>(view)) {
        cutout_path = corner->GetBackgroundPath(bounds);
      } else if (view->background() &&
                 view->background()->IsA<CustomCornersBackground>()) {
        cutout_path = view->background()
                          ->AsA<CustomCornersBackground>()
                          ->GetBackgroundPath(bounds, nullptr);
      } else {
        cutout_path = SkPath::Rect(gfx::RectToSkRect(bounds));
      }
      new_cutout_paths.push_back(cutout_path);
    } else {
      const auto* const background =
          std::get<InverseOf>(cutout).background.get();
      const gfx::Rect bounds = views::View::ConvertRectFromScreen(
          &*view_, background->view_->GetBoundsInScreen());
      for (SkPath& path : background->GetCornerPaths(bounds)) {
        new_cutout_paths.push_back(path);
      }
    }
  }

  if (cutout_paths_ == new_cutout_paths) {
    return;
  }

  cutout_paths_ = std::move(new_cutout_paths);
  view_->SchedulePaint();
}

void CustomCornersBackground::Paint(gfx::Canvas* canvas,
                                    views::View* view) const {
  if (!visible_) {
    return;
  }

  if (view->layer()) {
    CHECK(!view->layer()->fills_bounds_opaquely());
  }

  gfx::ScopedCanvas scoped_canvas(canvas);

  for (auto& cutout_path : cutout_paths_) {
    canvas->ClipPath(cutout_path, true, SkClipOp::kDifference);
  }

  gfx::Rect rect(view->GetLocalBounds());

  const VisualCorners corners = GetMirroredCorners();
  const Outline outline = GetMirroredOutline();

  // Function for maybe clipping a non-opaque corner with background's
  // background.
  const auto maybe_clip = [this](gfx::Canvas* canvas, VisualCorner corner,
                                 const gfx::Rect& bounds) {
    static constexpr gfx::Insets kCurveCutoutInsets(1);
    std::unique_ptr<gfx::ScopedCanvas> scoped;
    if (!primary_color_.is_opaque()) {
      scoped = std::make_unique<gfx::ScopedCanvas>(canvas);
      gfx::Rect cutout_bounds = bounds;
      cutout_bounds.Inset(-kCurveCutoutInsets);
      canvas->ClipPath(GetCornerPath(corner, cutout_bounds, kCurveCutoutInsets),
                       true);
    }
    return scoped;
  };

  // Draw corners behind where necessary using the background color.
  if (corners[VisualCorner::kTopLeft].type ==
      CornerType::kRoundedWithBackground) {
    const int corner_radius =
        corners[VisualCorner::kTopLeft].radius.value_or(default_radius_);
    const gfx::Rect bounds(0, 0, corner_radius, corner_radius);
    const SkPath corner_path = SkPath::Rect(gfx::RectToSkRect(bounds));
    const auto scope = maybe_clip(canvas, VisualCorner::kTopLeft, bounds);
    PaintPath(canvas, corner_path, corner_color_, /*anti_alias=*/false);
  }
  if (corners[VisualCorner::kTopRight].type ==
      CornerType::kRoundedWithBackground) {
    const int corner_radius =
        corners[VisualCorner::kTopRight].radius.value_or(default_radius_);
    const gfx::Rect bounds(rect.width() - corner_radius, 0, corner_radius,
                           corner_radius);
    const SkPath corner_path = SkPath::Rect(gfx::RectToSkRect(bounds));
    const auto scope = maybe_clip(canvas, VisualCorner::kTopRight, bounds);
    PaintPath(canvas, corner_path, corner_color_, /*anti_alias=*/false);
  }
  if (corners[VisualCorner::kBottomRight].type ==
      CornerType::kRoundedWithBackground) {
    const int corner_radius =
        corners[VisualCorner::kBottomRight].radius.value_or(default_radius_);
    const gfx::Rect bounds(rect.width() - corner_radius,
                           rect.height() - corner_radius, corner_radius,
                           corner_radius);
    const SkPath corner_path = SkPath::Rect(gfx::RectToSkRect(bounds));
    const auto scope = maybe_clip(canvas, VisualCorner::kBottomRight, bounds);
    PaintPath(canvas, corner_path, corner_color_, /*anti_alias=*/false);
  }
  if (corners[VisualCorner::kBottomLeft].type ==
      CornerType::kRoundedWithBackground) {
    const int corner_radius =
        corners[VisualCorner::kBottomLeft].radius.value_or(default_radius_);
    const gfx::Rect bounds(0, rect.height() - corner_radius, corner_radius,
                           corner_radius);
    const SkPath corner_path = SkPath::Rect(gfx::RectToSkRect(bounds));
    const auto scope = maybe_clip(canvas, VisualCorner::kBottomLeft, bounds);
    PaintPath(canvas, corner_path, corner_color_, /*anti_alias=*/false);
  }

  // Draw solid rect/rrect background:
  CornerRadii radii;
  PaintPath(canvas, GetBackgroundPath(view_->GetLocalBounds(), &radii),
            primary_color_, /*anti_alias=*/true);

  // Paint strokes around the outside. Corners get strokes if they are between
  // two sides with strokes and have a radius. Multiple paths may be drawn if
  // the sides with outlines are disconnected.
  if (outline.has_strokes()) {
    cc::PaintFlags stroke_flags;
    stroke_flags.setStrokeWidth(views::Separator::kThickness);
    SkColor color = GetView().GetColorProvider()->GetColor(outline.color.color);
    color = SkColorSetA(
        color, base::ClampRound(SkColorGetA(color) * outline.color.opacity));
    stroke_flags.setColor(color);
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
    if (outline.top) {
      stroke_path.moveTo(stroke_bounds.x() + radii[0].x(), stroke_bounds.y());
      stroke_path.lineTo(stroke_bounds.right() - radii[1].x(),
                         stroke_bounds.y());

      // Maybe draw the upper trailing corner as well.
      if (outline.trailing && !radii[1].isZero()) {
        stroke_path.arcTo(
            radii[1], 0, SkPathBuilder::kSmall_ArcSize, SkPathDirection::kCW,
            SkPoint(stroke_bounds.right(), stroke_bounds.y() + radii[1].y()));
      }
    }

    // Next, draw the right side if present.
    if (outline.trailing) {
      if (stroke_path.isEmpty()) {
        stroke_path.moveTo(stroke_bounds.right(),
                           stroke_bounds.y() + radii[1].y());
      }
      stroke_path.lineTo(stroke_bounds.right(),
                         stroke_bounds.bottom() - radii[2].y());

      // Maybe draw the bottom trailing corner.
      if (outline.bottom && !radii[2].isZero()) {
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
    if (outline.bottom) {
      if (stroke_path.isEmpty()) {
        stroke_path.moveTo(stroke_bounds.right() - radii[2].x(),
                           stroke_bounds.bottom());
      }
      stroke_path.lineTo(stroke_bounds.x() + radii[3].x(),
                         stroke_bounds.bottom());

      // Maybe draw the bottom leading corner.
      if (outline.leading && !radii[3].isZero()) {
        stroke_path.arcTo(
            radii[3], 0, SkPathBuilder::kSmall_ArcSize, SkPathDirection::kCW,
            SkPoint(stroke_bounds.x(), stroke_bounds.bottom() - radii[3].y()));
      }
    } else if (!stroke_path.isEmpty()) {
      // If there is no bottom, perhaps complete the current stroke.
      canvas->DrawPath(stroke_path.detach(), stroke_flags);
    }

    // Next, draw the left side if present.
    if (outline.leading) {
      if (stroke_path.isEmpty()) {
        stroke_path.moveTo(stroke_bounds.x(),
                           stroke_bounds.bottom() - radii[3].y());
      }

      // Maybe draw the top leading corner.
      stroke_path.lineTo(stroke_bounds.x(), stroke_bounds.y() + radii[0].y());
      if (outline.top && !radii[0].isZero()) {
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

void CustomCornersBackground::OnViewThemeChanged(views::View* view) {
  Background::OnViewThemeChanged(view);
  view_->SchedulePaint();
}

std::optional<gfx::RoundedCornersF>
CustomCornersBackground::GetRoundedCornerRadii() const {
  // Provided for completeness; this is not used anywhere.
  const VisualCorners corners = GetMirroredCorners();
  return gfx::RoundedCornersF(
      CornerToRadius(corners[VisualCorner::kTopLeft], default_radius_),
      CornerToRadius(corners[VisualCorner::kTopRight], default_radius_),
      CornerToRadius(corners[VisualCorner::kBottomRight], default_radius_),
      CornerToRadius(corners[VisualCorner::kBottomLeft], default_radius_));
}

const views::View& CustomCornersBackground::GetView() const {
  return *view_;
}

void CustomCornersBackground::OnBrowserPaintAsActiveChanged() {
  if (std::holds_alternative<FrameTheme>(primary_color_.color) ||
      std::holds_alternative<FrameTheme>(corner_color_.color)) {
    view_->SchedulePaint();
  }
}

void CustomCornersBackground::SchedulePaintHost() {
  view_->SchedulePaint();
}

CustomCornersBackground::VisualCorners
CustomCornersBackground::GetMirroredCorners() const {
  VisualCorners corners;
  for (auto i = 0; i < 4; ++i) {
    const CornerOrientation orientation = static_cast<CornerOrientation>(i);
    corners[ToVisualCorner(orientation)] = corners_[orientation];
  }
  return corners;
}

CustomCornersBackground::Outline CustomCornersBackground::GetMirroredOutline()
    const {
  Outline outline = outline_;
  if (base::i18n::IsRTL()) {
    std::swap(outline.leading, outline.trailing);
  }
  return outline;
}
