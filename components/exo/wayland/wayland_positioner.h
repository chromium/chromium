// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_WAYLAND_POSITIONER_H_
#define COMPONENTS_EXO_WAYLAND_WAYLAND_POSITIONER_H_

#include <xdg-shell-server-protocol.h>

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
  };

  // Represents the 1-dimensional projection of the gravity/anchor values.
  enum Direction { kNegative = -1, kNeutral = 0, kPositive = 1 };

  WaylandPositioner() = default;

  WaylandPositioner(const WaylandPositioner&) = delete;
  WaylandPositioner& operator=(const WaylandPositioner&) = delete;

  // Calculate and return bounds from current state.
  Result CalculateBounds(const gfx::Rect& work_area) const;

  void SetSize(gfx::Size size) { size_ = std::move(size); }

  void SetAnchorRect(gfx::Rect anchor_rect) {
    anchor_rect_ = std::move(anchor_rect);
  }

  void SetAnchor(uint32_t anchor);

  void SetGravity(uint32_t gravity);

  void SetAdjustment(uint32_t adjustment) { adjustment_ = adjustment; }

  void SetOffset(gfx::Vector2d offset) { offset_ = std::move(offset); }

 private:
  gfx::Size size_;

  gfx::Rect anchor_rect_;

  Direction anchor_x_ = kNeutral;
  Direction anchor_y_ = kNeutral;

  Direction gravity_x_ = kNeutral;
  Direction gravity_y_ = kNeutral;

  // A bitmask that defines the subset of modifications to the position/size
  // that are allowed, see xdg_positioner.constraint_adjustment() for more
  // details.
  uint32_t adjustment_ = XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_NONE;

  // Defines an absolute translation (i.e. unaffected by flipping, scaling or
  // resizing) for the placement of the window relative to the |anchor_rect_|.
  // See xdg_positioner.set_offset() for more details.
  gfx::Vector2d offset_;
};

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_WAYLAND_POSITIONER_H_
