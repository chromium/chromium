// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_ENVIRONMENT_GRID_H_
#define CHROME_BROWSER_VR_ELEMENTS_ENVIRONMENT_GRID_H_

#include "chrome/browser/vr/elements/rect.h"
#include "chrome/browser/vr/model/camera_model.h"
#include "third_party/skia/include/core/SkColor.h"

namespace vr {

class UiElementRenderer;

// Draws a quad with a radial gradient and grid lines.
class Grid : public Rect {
 public:
  Grid();

  Grid(const Grid&) = delete;
  Grid& operator=(const Grid&) = delete;

  ~Grid() override;

  void Render(UiElementRenderer* renderer,
              const CameraModel& model) const override;

  SkColor grid_color() const { return grid_color_; }
  void SetGridColor(SkColor grid_color);

  void OnColorAnimated(const SkColor& color,
                       int target_property_id,
                       gfx::KeyframeModel* keyframe_model) override;

  int gridline_count() const { return gridline_count_; }
  void set_gridline_count(int gridline_count) {
    gridline_count_ = gridline_count;
  }

 private:
  SkColor grid_color_ = SK_ColorWHITE;
  int gridline_count_ = 1;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_ENVIRONMENT_GRID_H_
