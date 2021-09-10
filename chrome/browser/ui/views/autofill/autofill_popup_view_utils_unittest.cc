// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_popup_view_utils.h"

#include "base/cxx17_backports.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(AutofillPopupViewUtilsTest, CalculatePopupBounds) {
  // Define the prompt sizes.
  const int desired_prompt_width = 40;
  const int desired_prompt_height = 30;
  // Convenience instance.
  const gfx::Size desired_size(desired_prompt_width, desired_prompt_height);

  // Define the dimensions of the input element.
  const int element_width = 20;
  const int element_height = 5;
  const int element_half_width = element_width / 2;

  // Define the default content area.
  const int default_content_area_bound_x = 0;
  const int default_content_area_bound_width = 100;
  const int default_content_area_bound_y = 0;
  const int default_content_area_bound_height = 100;

  const int most_right_position =
      default_content_area_bound_width - desired_prompt_width;
  const int most_left_position = 0;

  // Defines the x dimension of a test case.
  struct XDimensionCase {
    bool horizontally_centered;
    bool is_rtl;
    int element_bound_x;
    int expected_prompt_bound_x;
    // For most test scenarios, the default values are sufficient.
    int content_area_bound_x = default_content_area_bound_x;
    int content_area_bound_width = default_content_area_bound_width;
    int expected_prompt_width = desired_prompt_width;
  } x_dimension_cases[] = {
      // Base case, there is enough space to grow to the right and left for RTL.
      {false, false, 50, 50},
      {false, true, 50, 50 + element_width - desired_prompt_width},
      {true, false, 50, 50 + element_half_width},
      {true, true, 50, 50 + element_half_width - desired_prompt_width},
      // Corner cases: There is not sufficient space to grow to the right it LTR
      // or to the left in RTL.
      // Without centering, this gets right-aligned with the prompt.
      {false, false, 70, 50},
      {false, false, 100, most_right_position},
      {true, false, 70, most_right_position},
      {true, false, 100, most_right_position},
      // LTR extreme case: The field is outside of the view port
      {false, false, 120, most_right_position},
      {true, false, 120, most_right_position},
      // RTL corner case: there is not sufficient space to grow to the left.
      {true, true, 30, most_left_position},
      {true, true, 10, most_left_position},
      // Without centering, this gets right-aligned with the prompt.
      {false, true, 30, 10},
      // But it gets right-aligned if there is not enough space.
      {false, true, 10, 10},
      // RTL extreme case: The field is outside of the viewport
      {false, true, -10, most_left_position},
      {true, true, -10, most_left_position},
      // Special case: There is not enough space for the desired width.
      {true, true, 10, most_left_position, 0, 30, 30},
      // Without centering the prompt get left-aligned.
      {false, true, 10, 0, 0, 30, 30}};

  // Defines the y dimension of the test case.
  struct YDimensionCase {
    int element_bound_y;
    int expected_prompt_bound_y;
    // For most test scenarios, the default values are sufficient.
    int content_area_bound_y = default_content_area_bound_y;
    int content_area_bound_height = default_content_area_bound_height;
    int expected_prompt_height = desired_prompt_height;
  } y_dimension_cases[] = {
      // Base base, there is enough space to grow to the bottom.
      {50, 50 + element_height},
      // Corner cases, there is not enough space to grow to the top.
      {10, 10 + element_height},
      {0, 0 + element_height},
      {90, 90 - desired_prompt_height},
      {100, 100 - desired_prompt_height},
      // Extreme case: The field is outside of the viewport.
      {120, 100 - desired_prompt_height},
      // Special case: There is not enough space for the desired height.
      {0, 0 + element_height, 0, 30, 30 - element_height},
      {5, 5 + element_height, 0, 30, 25 - element_height}};

  for (const auto& x_dim : x_dimension_cases) {
    for (const auto& y_dim : y_dimension_cases) {
      gfx::Rect expected_popup_bounds(
          x_dim.expected_prompt_bound_x, y_dim.expected_prompt_bound_y,
          x_dim.expected_prompt_width, y_dim.expected_prompt_height);

      gfx::Rect content_area_bounds(
          x_dim.content_area_bound_x, y_dim.content_area_bound_y,
          x_dim.content_area_bound_width, y_dim.content_area_bound_height);
      gfx::Rect element_bounds(x_dim.element_bound_x, y_dim.element_bound_y,
                               element_width, element_height);

      gfx::Rect actual_popup_bounds = CalculatePopupBounds(
          desired_size, content_area_bounds, element_bounds, x_dim.is_rtl,
          /*horizontally_centered=*/x_dim.horizontally_centered);
      EXPECT_EQ(expected_popup_bounds, actual_popup_bounds);
    }
  }
}

TEST(AutofillPopupViewUtilsTest, NotEnoughHeightForAnItem) {
  // In this test, each row of the popup has a height of 8 pixels, and there is
  // no enough height in the content area to show one row.
  //
  //  |---------------------|    ---> y = 5
  //  |       Window        |
  //  | |-----------------| |    ---> y = 7
  //  | |   Content Area  | |
  //  | | |-------------| | |    ---> y = 8
  //  | | |   Element   | | |
  //  |-|-|-------------|-|-|    ---> y = 15

  constexpr int item_height = 8;
  constexpr int window_y = 5;
  constexpr int x = 10;
  constexpr int width = 5;
  constexpr int height = 10;

  gfx::Rect content_area_bounds(x, window_y + 2, width, height - 2);
  gfx::Rect element_bounds(x, window_y + 3, width, height - 3);

  EXPECT_FALSE(
      CanShowDropdownHere(item_height, content_area_bounds, element_bounds));
}

TEST(AutofillPopupViewUtilsTest, ElementOutOfContentAreaBounds) {
  // In this test, each row of the popup has a height of 8 pixels, and there is
  // no enough height in the content area to show one row.
  //
  //  |---------------------|    ---> y = 5
  //  |       Window        |
  //  | |-----------------| |    ---> y = 7
  //  | |                 | |
  //  | |   Content Area  | |
  //  | |                 | |
  //  |-|-----------------|-|    ---> y = 50
  //      |-------------|        ---> y = 53
  //      |   Element   |
  //      |-------------|        ---> y = 63

  constexpr int item_height = 8;
  constexpr int window_y = 5;
  constexpr int x = 10;
  constexpr int width = 5;
  constexpr int height = 46;

  gfx::Rect content_area_bounds(x, window_y + 2, width, height - 2);
  gfx::Rect element_bounds(x, window_y + height + 3, width, 10);

  EXPECT_FALSE(
      CanShowDropdownHere(item_height, content_area_bounds, element_bounds));
}
