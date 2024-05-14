// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_rounded_corner.h"

#include <memory>

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_background.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/canvas.h"

SidePanelRoundedCorner::SidePanelRoundedCorner(BrowserView* browser_view)
    : browser_view_(browser_view) {
  SetBackground(std::make_unique<TopContainerBackground>(browser_view_));
  SetPaintToLayer();
}

void SidePanelRoundedCorner::Layout(PassKey) {
  LayoutSuperclass<views::View>(this);
  SkPath path;
  if (browser_view_->unified_side_panel()) {
    bool is_right_aligned =
        browser_view_->unified_side_panel()->IsRightAligned();
    const float corner_radius = GetLayoutProvider()->GetCornerRadiusMetric(
        views::ShapeContextTokens::kSidePanelPageContentRadius);
    const gfx::Rect local_bounds = GetLocalBounds();
    if (is_right_aligned) {
      path.moveTo(0, 0);
      path.lineTo(local_bounds.width(), 0);
      path.lineTo(local_bounds.width(), local_bounds.height());
      path.lineTo(local_bounds.width() - views::Separator::kThickness,
                  local_bounds.height());
      path.arcTo(corner_radius, corner_radius, 0, SkPath::kSmall_ArcSize,
                 SkPathDirection::kCCW, 0, views::Separator::kThickness);
      path.lineTo(0, 0);
    } else {
      path.moveTo(0, 0);
      path.lineTo(local_bounds.width(), 0);
      path.lineTo(local_bounds.width(), views::Separator::kThickness);
      path.arcTo(corner_radius, corner_radius, 0, SkPath::kSmall_ArcSize,
                 SkPathDirection::kCCW, views::Separator::kThickness,
                 local_bounds.height());
      path.lineTo(0, local_bounds.height());
      path.lineTo(0, 0);
    }
  }
  SetClipPath(path);
}

void SidePanelRoundedCorner::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  cc::PaintFlags flags;
  flags.setStrokeWidth(views::Separator::kThickness * 2);
  flags.setColor(
      GetColorProvider()->GetColor(kColorToolbarContentAreaSeparator));
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setAntiAlias(true);

  bool is_right_aligned = browser_view_->unified_side_panel()->IsRightAligned();
  const float corner_radius = GetLayoutProvider()->GetCornerRadiusMetric(
      views::ShapeContextTokens::kSidePanelPageContentRadius);
  const gfx::Rect local_bounds = GetLocalBounds();
  SkPath path;
  if (is_right_aligned) {
    path.moveTo(local_bounds.width() - views::Separator::kThickness,
                local_bounds.height());
    path.arcTo(corner_radius, corner_radius, 0, SkPath::kSmall_ArcSize,
               SkPathDirection::kCCW, 0, views::Separator::kThickness);
  } else {
    path.moveTo(local_bounds.width(), views::Separator::kThickness);
    path.arcTo(corner_radius, corner_radius, 0, SkPath::kSmall_ArcSize,
               SkPathDirection::kCCW, views::Separator::kThickness,
               local_bounds.height());
  }
  canvas->DrawPath(path, flags);
}

void SidePanelRoundedCorner::OnThemeChanged() {
  SchedulePaint();
  View::OnThemeChanged();
}

BEGIN_METADATA(SidePanelRoundedCorner)
END_METADATA
