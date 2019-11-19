// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/wayland_positioner.h"

namespace exo {
namespace wayland {

namespace {

// Represents the 1-dimensional projection of the gravity/anchor values.
enum Direction { kNegative = -1, kNeutral = 0, kPositive = 1 };

static Direction Flip(Direction d) {
  return (Direction)-d;
}

// Decodes a masked anchor/gravity bitfield to the direction.
Direction MaskToDirection(uint32_t field,
                          uint32_t negative_mask,
                          uint32_t positive_mask) {
  DCHECK(!((field & negative_mask) && (field & positive_mask)));
  if (field & negative_mask)
    return kNegative;
  if (field & positive_mask)
    return kPositive;
  return kNeutral;
}

// Represents the possible/actual positioner adjustments for this window.
struct ConstraintAdjustment {
  bool flip;
  bool slide;
  bool resize;
};

// Decodes an adjustment bit field into the structure.
ConstraintAdjustment MaskToConstraintAdjustment(uint32_t field,
                                                uint32_t flip_mask,
                                                uint32_t slide_mask,
                                                uint32_t resize_mask) {
  return {field & flip_mask, field & slide_mask, field & resize_mask};
}

// A 1-dimensional projection of a range (a.k.a. a segment), used to solve the
// positioning problem in 1D.
struct Range1D {
  int32_t start;
  int32_t end;

  Range1D GetTranspose(int32_t offset) const {
    return {start + offset, end + offset};
  }

  int32_t center() const { return (start + end) / 2; }
};

// Works out the range's position that results from using exactly the
// adjustments specified by |adjustments|.
Range1D Calculate(const ConstraintAdjustment& adjustments,
                  int32_t work_size,
                  Range1D anchor_range,
                  uint32_t size,
                  int32_t offset,
                  Direction anchor,
                  Direction gravity) {
  if (adjustments.flip) {
    return Calculate({/*flip=*/false, adjustments.slide, adjustments.resize},
                     work_size, anchor_range, size, -offset, Flip(anchor),
                     Flip(gravity));
  }
  if (adjustments.resize) {
    Range1D unresized =
        Calculate({/*flip=*/false, adjustments.slide, /*resize=*/false},
                  work_size, anchor_range, size, offset, anchor, gravity);
    return {std::max(unresized.start, 0), std::min(unresized.end, work_size)};
  }
  if (adjustments.slide) {
    // Either the slide unconstrains the window, or the window is constrained
    // in the positive direction
    Range1D unslid =
        Calculate({/*flip=*/false, /*slide=*/false, /*resize=*/false},
                  work_size, anchor_range, size, offset, anchor, gravity);
    if (unslid.end > work_size)
      unslid = unslid.GetTranspose(work_size - unslid.end);
    if (unslid.start < 0)
      return unslid.GetTranspose(-unslid.start);
    return unslid;
  }

  int32_t start = offset;
  switch (anchor) {
    case Direction::kNegative:
      start += anchor_range.start;
      break;
    case Direction::kNeutral:
      start += anchor_range.center();
      break;
    case Direction::kPositive:
      start += anchor_range.end;
      break;
  }

  switch (gravity) {
    case Direction::kNegative:
      start -= size;
      break;
    case Direction::kNeutral:
      start -= size / 2;
      break;
    case Direction::kPositive:
      break;
  }
  return {start, start + size};
}

// Determines which adjustments (subject to them being a subset of the allowed
// adjustments) result in the best range position.
//
// Note: this is a 1-dimensional projection of the window-positioning problem.
std::pair<Range1D, ConstraintAdjustment> DetermineBestConstraintAdjustment(
    const Range1D& work_area,
    const Range1D& anchor_range,
    uint32_t size,
    int32_t offset,
    Direction anchor,
    Direction gravity,
    const ConstraintAdjustment& valid_adjustments) {
  if (work_area.start != 0) {
    int32_t shift = -work_area.start;
    std::pair<Range1D, ConstraintAdjustment> shifted_result =
        DetermineBestConstraintAdjustment(
            work_area.GetTranspose(shift), anchor_range.GetTranspose(shift),
            size, offset, anchor, gravity, valid_adjustments);
    return {shifted_result.first.GetTranspose(-shift), shifted_result.second};
  }

  // To determine the position, cycle through the available combinations of
  // adjustments and choose the first one that maximizes the amount of the
  // window that is visible on screen.
  Range1D best_position{0, 0};
  ConstraintAdjustment best_adjustments;
  bool best_constrained = true;
  int32_t best_visibility = 0;

  for (uint32_t adjustment_bit_field = 0; adjustment_bit_field < 8;
       ++adjustment_bit_field) {
    // When several options tie for visibility, we preference based on the
    // ordering flip > slide > resize, which is defined in the positioner
    // specification.
    ConstraintAdjustment adjustment =
        MaskToConstraintAdjustment(adjustment_bit_field, /*flip_mask=*/1,
                                   /*slide_mask=*/2, /*resize_mask=*/4);
    if ((adjustment.flip && !valid_adjustments.flip) ||
        (adjustment.slide && !valid_adjustments.slide) ||
        (adjustment.resize && !valid_adjustments.resize))
      continue;

    Range1D position = Calculate(adjustment, work_area.end, anchor_range, size,
                                 offset, anchor, gravity);
    bool constrained = position.start < 0 || position.end > work_area.end;
    int32_t visibility = std::abs(std::min(position.end, work_area.end) -
                                  std::max(position.start, 0));
    if (visibility > best_visibility || ((!constrained) && best_constrained)) {
      best_position = position;
      best_constrained = constrained;
      best_visibility = visibility;
      best_adjustments = adjustment;
    }
  }
  return {best_position, best_adjustments};
}

}  // namespace

WaylandPositioner::Result WaylandPositioner::CalculatePosition(
    const gfx::Rect& work_area,
    bool flip_x,
    bool flip_y) const {
  Direction anchor_x = MaskToDirection(anchor_, ZXDG_POSITIONER_V6_ANCHOR_LEFT,
                                       ZXDG_POSITIONER_V6_ANCHOR_RIGHT);
  Direction anchor_y = MaskToDirection(anchor_, ZXDG_POSITIONER_V6_ANCHOR_TOP,
                                       ZXDG_POSITIONER_V6_ANCHOR_BOTTOM);
  Direction gravity_x =
      MaskToDirection(gravity_, ZXDG_POSITIONER_V6_GRAVITY_LEFT,
                      ZXDG_POSITIONER_V6_GRAVITY_RIGHT);
  Direction gravity_y =
      MaskToDirection(gravity_, ZXDG_POSITIONER_V6_GRAVITY_TOP,
                      ZXDG_POSITIONER_V6_GRAVITY_BOTTOM);

  ConstraintAdjustment adjustments_x = MaskToConstraintAdjustment(
      adjustment_, ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_FLIP_X,
      ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_SLIDE_X,
      ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_RESIZE_X);
  ConstraintAdjustment adjustments_y = MaskToConstraintAdjustment(
      adjustment_, ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_FLIP_Y,
      ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_SLIDE_Y,
      ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_RESIZE_Y);

  int32_t offset_x = offset_.x();
  int32_t offset_y = offset_.y();

  // Chrome windows have the behaviour that if a menu needs to be flipped,
  // its children will be flipped by default. That is not part of the normal
  // wayland spec but we are doing it here for consistency.
  if (flip_x) {
    offset_x = -offset_x;
    anchor_x = Flip(anchor_x);
    gravity_x = Flip(gravity_x);
  }
  if (flip_y) {
    offset_y = -offset_y;
    anchor_y = Flip(anchor_y);
    gravity_y = Flip(gravity_y);
  }

  // Exo overrides the ability to slide in cases when the orthogonal
  // anchor+gravity would mean the slide can occlude |anchor_rect_|.
  //
  // We are doing this in order to stop a common case of clients allowing
  // dropdown menus to occlude the menu header. Whilst this may cause some
  // popups to avoid sliding where they could, for UX reasons we'd rather that
  // than allowing menus to be occluded.
  if (!(anchor_x == gravity_x && anchor_x != kNeutral))
    adjustments_y.slide = false;
  if (!(anchor_y == gravity_y && anchor_y != kNeutral))
    adjustments_x.slide = false;

  std::pair<Range1D, ConstraintAdjustment> x =
      DetermineBestConstraintAdjustment(
          {work_area.x(), work_area.right()},
          {anchor_rect_.x(), anchor_rect_.right()}, size_.width(), offset_x,
          anchor_x, gravity_x, adjustments_x);
  std::pair<Range1D, ConstraintAdjustment> y =
      DetermineBestConstraintAdjustment(
          {work_area.y(), work_area.bottom()},
          {anchor_rect_.y(), anchor_rect_.bottom()}, size_.height(), offset_y,
          anchor_y, gravity_y, adjustments_y);
  return {{x.first.start, y.first.start},
          {x.first.end - x.first.start, y.first.end - y.first.start},
          x.second.flip ? !flip_x : flip_x,
          y.second.flip ? !flip_y : flip_y};
}

}  // namespace wayland
}  // namespace exo
