// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/multi_contents_background_view.h"

#include "chrome/browser/ui/views/frame/top_container_background.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rounded_corners_f.h"

MultiContentsBackgroundView::MultiContentsBackgroundView(
    BrowserView* browser_view)
    : browser_view_(browser_view) {
  SetPaintToLayer(ui::LAYER_SOLID_COLOR);
}

MultiContentsBackgroundView::~MultiContentsBackgroundView() = default;

void MultiContentsBackgroundView::SetRoundedCorners(
    const gfx::RoundedCornersF& radii) {
  layer()->SetRoundedCornerRadius(radii);
  layer()->SetIsFastRoundedCorner(!radii.IsEmpty());
}

const gfx::RoundedCornersF& MultiContentsBackgroundView::GetRoundedCorners()
    const {
  return layer()->rounded_corner_radii();
}

void MultiContentsBackgroundView::OnThemeChanged() {
  views::View::OnThemeChanged();
  if (auto new_type = CalculateLayerType(); new_type != layer()->type()) {
    SetPaintToLayer(new_type);
  }

  if (layer()->type() == ui::LAYER_SOLID_COLOR) {
    UpdateSolidLayerColor();
  } else {
    SchedulePaint();
  }
}

void MultiContentsBackgroundView::OnPaint(gfx::Canvas* canvas) {
  CHECK_EQ(layer()->type(), ui::LAYER_TEXTURED);
  TopContainerBackground::PaintBackground(canvas, this, browser_view_);
}

ui::LayerType MultiContentsBackgroundView::CalculateLayerType() const {
  const bool has_custom_image =
      !TopContainerBackground::GetBackgroundColor(this, browser_view_)
           .has_value();
  return has_custom_image ? ui::LAYER_TEXTURED : ui::LAYER_SOLID_COLOR;
}

void MultiContentsBackgroundView::UpdateSolidLayerColor() {
  CHECK_EQ(layer()->type(), ui::LAYER_SOLID_COLOR);
  if (auto color =
          TopContainerBackground::GetBackgroundColor(this, browser_view_)) {
    layer()->SetColor(*color);
  }
}

BEGIN_METADATA(MultiContentsBackgroundView)
ADD_PROPERTY_METADATA(gfx::RoundedCornersF, RoundedCorners)
END_METADATA
