// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/environment/grid.h"

#include "chrome/browser/vr/target_property.h"
#include "chrome/browser/vr/ui_element_renderer.h"

namespace vr {

Grid::Grid() {}
Grid::~Grid() {}

void Grid::SetGridColor(SkColor color) {
  animator().TransitionColorTo(this, last_frame_time(), GRID_COLOR, grid_color_,
                               color);
}

void Grid::OnColorAnimated(const SkColor& color,
                           int target_property_id,
                           gfx::KeyframeModel* keyframe_model) {
  if (target_property_id == GRID_COLOR) {
    grid_color_ = color;
  } else {
    Rect::OnColorAnimated(color, target_property_id, keyframe_model);
  }
}

void Grid::Render(UiElementRenderer* renderer, const CameraModel& model) const {
  renderer->DrawGradientGridQuad(
      model.view_proj_matrix * world_space_transform(), grid_color_,
      gridline_count_, computed_opacity());
}

}  // namespace vr
