// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_WAYLAND_POSITIONER_H_
#define COMPONENTS_EXO_WAYLAND_WAYLAND_POSITIONER_H_

#include <xdg-shell-unstable-v6-server-protocol.h>

#include "base/macros.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"

namespace exo {
namespace wayland {

class WaylandPositioner {
 public:
  // Holds the result of window positioning.
  struct Result {
    gfx::Point origin;
    gfx::Size size;
    bool x_flipped;
    bool y_flipped;
  };

  WaylandPositioner() = default;

  // Calculate and return position from current state.
  Result CalculatePosition(const gfx::Rect& work_area,
                           bool flip_x,
                           bool flip_y) const;

  void SetSize(gfx::Size size) { size_ = std::move(size); }

  void SetAnchorRect(gfx::Rect anchor_rect) {
    anchor_rect_ = std::move(anchor_rect);
  }

  void SetAnchor(uint32_t anchor) { anchor_ = anchor; }

  void SetGravity(uint32_t gravity) { gravity_ = gravity; }

  void SetAdjustment(uint32_t adjustment) { adjustment_ = adjustment; }

  void SetOffset(gfx::Vector2d offset) { offset_ = std::move(offset); }

 private:
  gfx::Size size_;

  gfx::Rect anchor_rect_;

  uint32_t anchor_ = ZXDG_POSITIONER_V6_ANCHOR_NONE;

  uint32_t gravity_ = ZXDG_POSITIONER_V6_GRAVITY_NONE;

  // A bitmask that defines the subset of modifications to the position/size
  // that are allowed, see zxdg_positioner.constraint_adjustment() for more
  // details.
  uint32_t adjustment_ = ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_NONE;

  // Defines an absolute translation (i.e. unaffected by flipping, scaling or
  // resizing) for the placement of the window relative to the |anchor_rect_|.
  // See zxdg_positioner.set_offset() for more details.
  gfx::Vector2d offset_;

  DISALLOW_COPY_AND_ASSIGN(WaylandPositioner);
};

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_WAYLAND_POSITIONER_H_
