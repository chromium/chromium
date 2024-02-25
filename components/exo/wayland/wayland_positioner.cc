// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/wayland_positioner.h"

#include <numeric>
#include <ostream>

namespace exo::wayland {

namespace {

std::pair<WaylandPositioner::Direction, WaylandPositioner::Direction>
DecomposeAnchor(uint32_t anchor) {
  switch (anchor) {
    default:
    case XDG_POSITIONER_ANCHOR_NONE:
      return std::make_pair(WaylandPositioner::Direction::kNeutral,
                            WaylandPositioner::Direction::kNeutral);
    case XDG_POSITIONER_ANCHOR_TOP:
      return std::make_pair(WaylandPositioner::Direction::kNeutral,
                            WaylandPositioner::Direction::kNegative);
    case XDG_POSITIONER_ANCHOR_BOTTOM:
      return std::make_pair(WaylandPositioner::Direction::kNeutral,
                            WaylandPositioner::Direction::kPositive);
    case XDG_POSITIONER_ANCHOR_LEFT:
      return std::make_pair(WaylandPositioner::Direction::kNegative,
                            WaylandPositioner::Direction::kNeutral);
    case XDG_POSITIONER_ANCHOR_RIGHT:
      return std::make_pair(WaylandPositioner::Direction::kPositive,
                            WaylandPositioner::Direction::kNeutral);
    case XDG_POSITIONER_ANCHOR_TOP_LEFT:
      return std::make_pair(WaylandPositioner::Direction::kNegative,
                            WaylandPositioner::Direction::kNegative);
    case XDG_POSITIONER_ANCHOR_BOTTOM_LEFT:
      return std::make_pair(WaylandPositioner::Direction::kNegative,
                            WaylandPositioner::Direction::kPositive);
    case XDG_POSITIONER_ANCHOR_TOP_RIGHT:
      return std::make_pair(WaylandPositioner::Direction::kPositive,
                            WaylandPositioner::Direction::kNegative);
    case XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT:
      return std::make_pair(WaylandPositioner::Direction::kPositive,
                            WaylandPositioner::Direction::kPositive);
  }
}

std::pair<WaylandPositioner::Direction, WaylandPositioner::Direction>
DecomposeGravity(uint32_t gravity) {
  switch (gravity) {
    default:
    case XDG_POSITIONER_GRAVITY_NONE:
      return std::make_pair(WaylandPositioner::Direction::kNeutral,
                            WaylandPositioner::Direction::kNeutral);
    case XDG_POSITIONER_GRAVITY_TOP:
      return std::make_pair(WaylandPositioner::Direction::kNeutral,
                            WaylandPositioner::Direction::kNegative);
    case XDG_POSITIONER_GRAVITY_BOTTOM:
      return std::make_pair(WaylandPositioner::Direction::kNeutral,
                            WaylandPositioner::Direction::kPositive);
    case XDG_POSITIONER_GRAVITY_LEFT:
      return std::make_pair(WaylandPositioner::Direction::kNegative,
                            WaylandPositioner::Direction::kNeutral);
    case XDG_POSITIONER_GRAVITY_RIGHT:
      return std::make_pair(WaylandPositioner::Direction::kPositive,
                            WaylandPositioner::Direction::kNeutral);
    case XDG_POSITIONER_GRAVITY_TOP_LEFT:
      return std::make_pair(WaylandPositioner::Direction::kNegative,
                            WaylandPositioner::Direction::kNegative);
    case XDG_POSITIONER_GRAVITY_BOTTOM_LEFT:
      return std::make_pair(WaylandPositioner::Direction::kNegative,
                            WaylandPositioner::Direction::kPositive);
    case XDG_POSITIONER_GRAVITY_TOP_RIGHT:
      return std::make_pair(WaylandPositioner::Direction::kPositive,
                            WaylandPositioner::Direction::kNegative);
    case XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT:
      return std::make_pair(WaylandPositioner::Direction::kPositive,
                            WaylandPositioner::Direction::kPositive);
  }
}

static WaylandPositioner::Direction Flip(WaylandPositioner::Direction d) {
  return (WaylandPositioner::Direction)-d;
}

// Represents the possible/actual positioner adjustments for this window.
struct ConstraintAdjustment {
  bool flip;
  bool slide;
  bool resize;

  bool allows_all() const { return flip && slide && resize; }
};

// Decodes an adjustment bit field into the structure.
ConstraintAdjustment MaskToConstraintAdjustment(uint32_t field,
                                                uint32_t flip_mask,
                                                uint32_t slide_mask,
                                                uint32_t resize_mask) {
  return {!!(field & flip_mask), !!(field & slide_mask),
          !!(field & resize_mask)};
}

// A 1-dimensional projection of a range (a.k.a. a segment), used to solve the
// positioning problem in 1D.
struct Range1D {
  int32_t start;
  int32_t end;

  Range1D GetTranspose(int32_t offset) const {
    return {start + offset, end + offset};
  }

  int32_t center() const { return std::midpoint(start, end); }
};

// Works out the range's position that results from using exactly the
// adjustments specified by |adjustments|.
Range1D Calculate(const ConstraintAdjustment& adjustments,
                  int32_t work_size,
                  Range1D anchor_range,
                  uint32_t size,
                  int32_t offset,
                  WaylandPositioner::Direction anchor,
                  WaylandPositioner::Direction gravity) {
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
    case WaylandPositioner::Direction::kNegative:
      start += anchor_range.start;
      break;
    case WaylandPositioner::Direction::kNeutral:
      start += anchor_range.center();
      break;
    case WaylandPositioner::Direction::kPositive:
      start += anchor_range.end;
      break;
  }

  switch (gravity) {
    case WaylandPositioner::Direction::kNegative:
      start -= size;
      break;
    case WaylandPositioner::Direction::kNeutral:
      start -= size / 2;
      break;
    case WaylandPositioner::Direction::kPositive:
      break;
  }
  return {start, static_cast<int32_t>(start + size)};
}

// The intermediate adjustment results when computing the best positioning for
// the popup.
struct IntermediateAdjustmentResult {
  // Result statistics for comparing two different placements.
  struct Stats {
    // If this is set to false, this result will be chosen iff it is the only
    // non-constrained option.
    bool preferred;
    bool constrained;
    int32_t visibility;
  } stats;
  Range1D position;
  ConstraintAdjustment adjustment;
};

// Determines which adjustments (subject to them being a subset of the allowed
// adjustments) result in the best range position.
//
// Note: this is a 1-dimensional projection of the window-positioning problem.
std::pair<Range1D, ConstraintAdjustment> DetermineBestConstraintAdjustment(
    const Range1D& work_area,
    const Range1D& anchor_range,
    uint32_t size,
    int32_t offset,
    WaylandPositioner::Direction anchor,
    WaylandPositioner::Direction gravity,
    const ConstraintAdjustment& valid_adjustments,
    bool avoid_occlusion) {
  if (work_area.start != 0) {
    int32_t shift = -work_area.start;
    std::pair<Range1D, ConstraintAdjustment> shifted_result =
        DetermineBestConstraintAdjustment(
            work_area.GetTranspose(shift), anchor_range.GetTranspose(shift),
            size, offset, anchor, gravity, valid_adjustments, avoid_occlusion);
    return {shifted_result.first.GetTranspose(-shift), shifted_result.second};
  }

  // To determine the position, cycle through the available combinations of
  // adjustments and choose the first one that maximizes the amount of the
  // window that is visible on screen. Preferences are given in accordance to
  // order when all the stats are equivalent. Therefore, the preference for
  // adjustment will be flip > slide > resize.
  IntermediateAdjustmentResult best{{/*preferred=*/false,
                                     /*constrained=*/true,
                                     /*visibility=*/0},
                                    /*position=*/{0, 0},
                                    /*adjustment=*/ConstraintAdjustment{}};
  bool found_solution = false;
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

    // When sliding, it can be possible to occlude the parent menu. Therefore,
    // this option should not be used if there are better options which have
    // acceptable placement.
    bool possible_occlusion = false;
    if (avoid_occlusion && adjustment.slide)
      possible_occlusion = true;

    Range1D position = Calculate(adjustment, work_area.end, anchor_range, size,
                                 offset, anchor, gravity);
    bool constrained = position.start < 0 || position.end > work_area.end;
    int32_t visibility = std::abs(std::min(position.end, work_area.end) -
                                  std::max(position.start, 0));

    bool preferred = !possible_occlusion && !constrained;
    bool is_better = false;
    if (preferred) {
      // Always choose a preferred adjustment if the best we have is not
      // preferred.
      if (!best.stats.preferred || visibility > best.stats.visibility)
        is_better = true;
    } else {
      if (!constrained && best.stats.constrained)
        is_better = true;
    }

    if (is_better) {
      found_solution = true;
      best = IntermediateAdjustmentResult{
          {preferred, constrained, visibility}, position, adjustment};
    }
  }

  // If no solution can be found, allow all transformations. Unfortunately the
  // default setting is not valid, because it has a 0x0 dimension.
  if (!found_solution && !valid_adjustments.allows_all()) {
    ConstraintAdjustment allow_all = {
        .flip = true,
        .slide = true,
        .resize = true,
    };
    return DetermineBestConstraintAdjustment(work_area, anchor_range, size,
                                             offset, anchor, gravity, allow_all,
                                             avoid_occlusion);
  }

  DCHECK(found_solution)
      << "Computation is returning without a valid solution. This will result "
         "in undefined placement.";
  return {best.position, best.adjustment};
}

}  // namespace

void WaylandPositioner::SetAnchor(uint32_t anchor) {
  std::pair<WaylandPositioner::Direction, WaylandPositioner::Direction>
      decompose;
  decompose = DecomposeAnchor(anchor);
  anchor_x_ = decompose.first;
  anchor_y_ = decompose.second;
}

void WaylandPositioner::SetGravity(uint32_t gravity) {
  std::pair<WaylandPositioner::Direction, WaylandPositioner::Direction>
      decompose;
  decompose = DecomposeGravity(gravity);
  gravity_x_ = decompose.first;
  gravity_y_ = decompose.second;
}

WaylandPositioner::Result WaylandPositioner::CalculateBounds(
    const gfx::Rect& work_area) const {
  auto anchor_x = anchor_x_;
  auto anchor_y = anchor_y_;
  auto gravity_x = gravity_x_;
  auto gravity_y = gravity_y_;

  ConstraintAdjustment adjustments_x = MaskToConstraintAdjustment(
      adjustment_, XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_X,
      XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X,
      XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_X);
  ConstraintAdjustment adjustments_y = MaskToConstraintAdjustment(
      adjustment_, XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y,
      XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y,
      XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_Y);

  int32_t offset_x = offset_.x();
  int32_t offset_y = offset_.y();

  // Exo may prefer some adjustments over others in cases when the orthogonal
  // anchor+gravity would mean the slide can occlude |anchor_rect_|, unless it
  // already is occluded.
  //
  // We are doing this in order to stop a common case of clients allowing
  // dropdown menus to occlude the menu header. Whilst this may cause some
  // popups to avoid sliding where they could, for UX reasons we'd rather that
  // than allowing menus to be occluded.
  //
  // This is best effort. If it is not possible to position the popup within the
  // work area, exo might choose to occlude the parent.
  bool x_occluded = !(anchor_x == gravity_x && anchor_x != kNeutral);
  bool y_occluded = !(anchor_y == gravity_y && anchor_y != kNeutral);
  bool avoid_y_occlusion = x_occluded && !y_occluded;
  bool avoid_x_occlusion = y_occluded && !x_occluded;

  std::pair<Range1D, ConstraintAdjustment> x =
      DetermineBestConstraintAdjustment(
          {work_area.x(), work_area.right()},
          {anchor_rect_.x(), anchor_rect_.right()}, size_.width(), offset_x,
          anchor_x, gravity_x, adjustments_x, avoid_x_occlusion);
  std::pair<Range1D, ConstraintAdjustment> y =
      DetermineBestConstraintAdjustment(
          {work_area.y(), work_area.bottom()},
          {anchor_rect_.y(), anchor_rect_.bottom()}, size_.height(), offset_y,
          anchor_y, gravity_y, adjustments_y, avoid_y_occlusion);
  gfx::Point origin(x.first.start, y.first.start);
  gfx::Size size(std::max(1, x.first.end - x.first.start),
                 std::max(1, y.first.end - y.first.start));
  return {origin, size};
}

}  // namespace exo::wayland
