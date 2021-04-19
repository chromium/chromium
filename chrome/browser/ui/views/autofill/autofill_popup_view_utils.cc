// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_popup_view_utils.h"

#include <algorithm>

// Returns:
//    |value| if |lower_bound| < |value| < |upper_bound|
//    |lower_bound| if |value| < |lower_bound| < |upper_bound|
//    |upper_bound| if |lower_bound| < |upper_bound| < |value|
int NormalizeValueBasedOnBounds(int lower_bound, int upper_bound, int value) {
  return std::max(lower_bound, std::min(upper_bound, value));
}

void CalculatePopupXAndWidth(int popup_preferred_width,
                             const gfx::Rect& window_bounds,
                             const gfx::Rect& element_bounds,
                             bool is_rtl,
                             gfx::Rect* popup_bounds) {
  int right_growth_start = NormalizeValueBasedOnBounds(
      window_bounds.x(), window_bounds.right(), element_bounds.x());
  int left_growth_end = NormalizeValueBasedOnBounds(
      window_bounds.x(), window_bounds.right(), element_bounds.right());

  int right_available = window_bounds.right() - right_growth_start;
  int left_available = left_growth_end - window_bounds.x();

  int popup_width = std::min(popup_preferred_width,
                             std::max(left_available, right_available));

  // Prefer to grow towards the end (right for LTR, left for RTL). But if there
  // is not enough space available in the desired direction and more space in
  // the other direction, reverse it.
  bool grow_left = false;
  if (is_rtl) {
    grow_left =
        left_available >= popup_width || left_available >= right_available;
  } else {
    grow_left =
        right_available < popup_width && right_available < left_available;
  }

  popup_bounds->set_width(popup_width);
  popup_bounds->set_x(grow_left ? left_growth_end - popup_width
                                : right_growth_start);
}

void CalculatePopupYAndHeight(int popup_preferred_height,
                              const gfx::Rect& window_bounds,
                              const gfx::Rect& element_bounds,
                              gfx::Rect* popup_bounds) {
  int top_growth_end = NormalizeValueBasedOnBounds(
      window_bounds.y(), window_bounds.bottom(), element_bounds.y());
  int bottom_growth_start = NormalizeValueBasedOnBounds(
      window_bounds.y(), window_bounds.bottom(), element_bounds.bottom());

  int top_available = top_growth_end - window_bounds.y();
  int bottom_available = window_bounds.bottom() - bottom_growth_start;

  popup_bounds->set_height(popup_preferred_height);
  popup_bounds->set_y(top_growth_end);

  if (bottom_available >= popup_preferred_height ||
      bottom_available >= top_available) {
    popup_bounds->AdjustToFit(
        gfx::Rect(popup_bounds->x(), element_bounds.bottom(),
                  popup_bounds->width(), bottom_available));
  } else {
    popup_bounds->AdjustToFit(gfx::Rect(popup_bounds->x(), window_bounds.y(),
                                        popup_bounds->width(), top_available));
  }
}

gfx::Rect CalculatePopupBounds(const gfx::Size& desired_size,
                               const gfx::Rect& window_bounds,
                               const gfx::Rect& element_bounds,
                               bool is_rtl) {
  gfx::Rect popup_bounds;

  CalculatePopupXAndWidth(desired_size.width(), window_bounds, element_bounds,
                          is_rtl, &popup_bounds);
  CalculatePopupYAndHeight(desired_size.height(), window_bounds, element_bounds,
                           &popup_bounds);

  return popup_bounds;
}

bool CanShowDropdownHere(int item_height,
                         const gfx::Rect& content_area_bounds,
                         const gfx::Rect& element_bounds) {
  // Ensure that at least one row of the popup will be displayed within the
  // bounds of the content area so that the user notices the presence of the
  // popup.
  bool enough_space_for_one_item_in_content_area_above_element =
      element_bounds.y() - content_area_bounds.y() >= item_height;
  bool element_top_is_within_content_area_bounds =
      element_bounds.y() > content_area_bounds.y() &&
      element_bounds.y() < content_area_bounds.bottom();

  bool enough_space_for_one_item_in_content_area_below_element =
      content_area_bounds.bottom() - element_bounds.bottom() >= item_height;
  bool element_bottom_is_within_content_area_bounds =
      element_bounds.bottom() > content_area_bounds.y() &&
      element_bounds.bottom() < content_area_bounds.bottom();

  return (enough_space_for_one_item_in_content_area_above_element &&
          element_top_is_within_content_area_bounds) ||
         (enough_space_for_one_item_in_content_area_below_element &&
          element_bottom_is_within_content_area_bounds);
}
