// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_INPUT_OVERLAY_ACTIONS_POSITION_H_
#define COMPONENTS_ARC_INPUT_OVERLAY_ACTIONS_POSITION_H_

#include "base/values.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace arc {
namespace input_overlay {
// This is the base position for location. It includes anchor point
// and the vector from the anchor point to the target position.
class Position {
 public:
  Position();
  Position(const Position&) = delete;
  Position& operator=(const Position&) = delete;
  virtual ~Position();

  // Json value format:
  // {
  //   "anchor_to_target": [
  //     0.1796875,
  //     0.25
  //   ]
  // }
  virtual bool ParseFromJson(const base::Value& value);
  // Return the position coords in window.
  virtual gfx::PointF CalculatePosition(const gfx::RectF& window_bounds);

  const gfx::PointF& anchor() const { return anchor_; }
  const gfx::Vector2dF& anchor_to_target() const { return anchor_to_target_; }

 private:
  // Default anchor_ is (0, 0). Anchor is the point position where the UI
  // position is relative to. For example, a UI may be always relative to the
  // left-bottom.
  gfx::PointF anchor_;
  // The anchor_to_target_ is the vector from anchor to target UI
  // position. The value may be negative if the direction is different from
  // original x and y.
  gfx::Vector2dF anchor_to_target_;
};

}  // namespace input_overlay
}  // namespace arc

#endif  // COMPONENTS_ARC_INPUT_OVERLAY_ACTIONS_POSITION_H_
