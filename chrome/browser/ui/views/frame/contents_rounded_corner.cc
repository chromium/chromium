// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/contents_rounded_corner.h"

#include <memory>

#include "base/functional/callback.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_background.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/layout_provider.h"

ContentsRoundedCorner::ContentsRoundedCorner(
    BrowserView* browser_view,
    views::ShapeContextTokens corner_radius_token,
    base::RepeatingCallback<bool()> is_right_aligned_callback)
    : corner_radius_token_(corner_radius_token),
      is_right_aligned_callback_(std::move(is_right_aligned_callback)) {
  SetBackground(std::make_unique<TopContainerBackground>(browser_view));
  SetPaintToLayer();
}

ContentsRoundedCorner::~ContentsRoundedCorner() = default;

gfx::Size ContentsRoundedCorner::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  // This can return nullptr when there is no Widget (for context, see
  // http://crbug.com/40178332). The nullptr dereference does not always
  // crash due to compiler optimizations, so CHECKing here ensures we crash.
  CHECK(GetLayoutProvider());
  const float corner_radius = GetLayoutProvider()->GetCornerRadiusMetric(
      views::ShapeContextTokens::kContentSeparatorRadius);
  const float corner_size = corner_radius + views::Separator::kThickness;
  return gfx::Size(corner_size, corner_size);
}

void ContentsRoundedCorner::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  SkPathBuilder path;
  const float corner_radius =
      GetLayoutProvider()->GetCornerRadiusMetric(corner_radius_token_);
  const gfx::Rect local_bounds = GetLocalBounds();
  if (is_right_aligned_callback_.Run()) {
    path.moveTo(0, 0);
    path.lineTo(local_bounds.width(), 0);
    path.lineTo(local_bounds.width(), local_bounds.height());
    path.lineTo(local_bounds.width() - views::Separator::kThickness,
                local_bounds.height());
    path.arcTo(SkVector(corner_radius, corner_radius), 0,
               SkPathBuilder::kSmall_ArcSize, SkPathDirection::kCCW,
               SkPoint(0, views::Separator::kThickness));
    path.lineTo(0, 0);
  } else {
    path.moveTo(0, 0);
    path.lineTo(local_bounds.width(), 0);
    path.lineTo(local_bounds.width(), views::Separator::kThickness);
    path.arcTo(SkVector(corner_radius, corner_radius), 0,
               SkPathBuilder::kSmall_ArcSize, SkPathDirection::kCCW,
               SkPoint(views::Separator::kThickness, local_bounds.height()));
    path.lineTo(0, local_bounds.height());
    path.lineTo(0, 0);
  }

  // TODO(b:460890881): Clip canvas during repaint instead using a mask layer.
  SetClipPath(path.detach());
}

void ContentsRoundedCorner::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  cc::PaintFlags flags;
  flags.setStrokeWidth(views::Separator::kThickness * 2);
  flags.setColor(
      GetColorProvider()->GetColor(kColorToolbarContentAreaSeparator));
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setAntiAlias(true);

  const float corner_radius =
      GetLayoutProvider()->GetCornerRadiusMetric(corner_radius_token_);
  const gfx::Rect local_bounds = GetLocalBounds();
  SkPathBuilder path;
  if (is_right_aligned_callback_.Run()) {
    path.moveTo(local_bounds.width() - views::Separator::kThickness,
                local_bounds.height());
    path.arcTo(SkVector(corner_radius, corner_radius), 0,
               SkPathBuilder::kSmall_ArcSize, SkPathDirection::kCCW,
               SkPoint(0, views::Separator::kThickness));
  } else {
    path.moveTo(local_bounds.width(), views::Separator::kThickness);
    path.arcTo(SkVector(corner_radius, corner_radius), 0,
               SkPathBuilder::kSmall_ArcSize, SkPathDirection::kCCW,
               SkPoint(views::Separator::kThickness, local_bounds.height()));
  }
  canvas->DrawPath(path.detach(), flags);
}

void ContentsRoundedCorner::OnThemeChanged() {
  SchedulePaint();
  View::OnThemeChanged();
}

BEGIN_METADATA(ContentsRoundedCorner)
END_METADATA
