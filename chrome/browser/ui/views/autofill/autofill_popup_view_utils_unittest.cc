
// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_popup_view_utils.h"

#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(AutofillPopupViewUtilsTest, CalculatePopupBounds) {
  constexpr int desired_width = 40;
  constexpr int desired_height = 16;

  gfx::Size preferred_size(desired_width, desired_height);

  struct {
    gfx::Rect element_bounds;
    gfx::Rect window_bounds;
    gfx::Rect expected_popup_bounds_ltr;
    // Non-empty only when it differs from the ltr expectation.
    gfx::Rect expected_popup_bounds_rtl;
  } test_cases[] = {
      // The popup grows down and to the end.
      {gfx::Rect(38, 0, 5, 0),
       gfx::Rect(0, 0, 2 * desired_width, 2 * desired_height),
       gfx::Rect(38, 0, desired_width, desired_height),
       gfx::Rect(3, 0, desired_width, desired_height)},

      // The popup grows down and to the left when there's no room on the right.
      {gfx::Rect(2 * desired_width, 0, 5, 0),
       gfx::Rect(0, 0, 2 * desired_width, 2 * desired_height),
       gfx::Rect(desired_width, 0, desired_width, desired_height)},

      // The popup grows up and to the right.
      {gfx::Rect(0, 2 * desired_height, 5, 0),
       gfx::Rect(0, 0, 2 * desired_width, 2 * desired_height),
       gfx::Rect(0, desired_height, desired_width, desired_height)},

      // The popup grows up and to the left.
      {gfx::Rect(2 * desired_width, 2 * desired_height, 5, 0),
       gfx::Rect(0, 0, 2 * desired_width, 2 * desired_height),
       gfx::Rect(desired_width, desired_height, desired_width, desired_height)},

      // The popup would be partial off the top and left side of the window.
      {gfx::Rect(-desired_width / 2, -desired_height / 2, 5, 0),
       gfx::Rect(0, 0, 2 * desired_width, 2 * desired_height),
       gfx::Rect(0, 0, desired_width, desired_height)},

      // The popup would be partially off the bottom and the right side of
      // the window.
      {gfx::Rect(1.5 * desired_width, 1.5 * desired_height, 5, 0),
       gfx::Rect(0, 0, 2 * desired_width, 2 * desired_height),
       gfx::Rect((1.5 * desired_width + 5 - desired_width),
                 (1.5 * desired_height - desired_height), desired_width,
                 desired_height)},

      // The popup grows down and to the right.
      {gfx::Rect(0, 1.75 * desired_height, 5, desired_height),
       gfx::Rect(0, 0, 2 * desired_width, 4 * desired_height),
       gfx::Rect(0, 2.75 * desired_height, desired_width, desired_height)},

      // The popup grows down and to the right.
      {gfx::Rect(0, 1.25 * desired_height, 5, desired_height),
       gfx::Rect(0, 0, 2 * desired_width, 4 * desired_height),
       gfx::Rect(0, 2.25 * desired_height, desired_width, desired_height)},

      // The popup grows down till the end and to the right.
      {gfx::Rect(0, 0, 5, desired_height),
       gfx::Rect(0, 0, 2 * desired_width, 1.5 * desired_height),
       gfx::Rect(0, desired_height, desired_width, desired_height / 2)},

      // The popup grows up till the end and to the right.
      {gfx::Rect(0, desired_height / 2, 5, desired_height),
       gfx::Rect(0, 0, 2 * desired_width, 1.5 * desired_height),
       gfx::Rect(0, 0, desired_width, desired_height / 2)},
  };

  for (size_t i = 0; i < base::size(test_cases); ++i) {
    gfx::Rect actual_popup_bounds =
        CalculatePopupBounds(preferred_size, test_cases[i].window_bounds,
                             test_cases[i].element_bounds, /* is_rtl= */ false);
    EXPECT_EQ(test_cases[i].expected_popup_bounds_ltr.ToString(),
              actual_popup_bounds.ToString())
        << "Popup bounds failed to match for ltr test " << i;

    actual_popup_bounds =
        CalculatePopupBounds(preferred_size, test_cases[i].window_bounds,
                             test_cases[i].element_bounds, /* is_rtl= */ true);
    gfx::Rect expected_popup_bounds = test_cases[i].expected_popup_bounds_rtl;
    if (expected_popup_bounds.IsEmpty())
      expected_popup_bounds = test_cases[i].expected_popup_bounds_ltr;
    EXPECT_EQ(expected_popup_bounds.ToString(), actual_popup_bounds.ToString())
        << "Popup bounds failed to match for rtl test " << i;
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
