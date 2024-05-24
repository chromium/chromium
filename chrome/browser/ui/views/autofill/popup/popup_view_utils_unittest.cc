// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_view_utils.h"

#include <vector>

#include "chrome/browser/ui/views/autofill/popup/popup_base_view.h"
#include "components/autofill/core/browser/ui/popup_open_enums.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

std::vector<views::BubbleArrowSide> GetDefaultPopupSides() {
  return {PopupBaseView::kDefaultPreferredPopupSides.begin(),
          PopupBaseView::kDefaultPreferredPopupSides.end()};
}

TEST(PopupViewsUtilsTest, GetOptimalArrowSide) {
  const gfx::Size default_preferred_size{200, 600};

  struct TestCase {
    views::BubbleArrowSide expected_arrow_side;
    gfx::Rect content_area_bounds;
    gfx::Rect element_bounds;
    gfx::Size preferred_size;
    std::vector<views::BubbleArrowSide> preferred_sides =
        GetDefaultPopupSides();
    PopupAnchorType anchor_type = PopupAnchorType::kField;
  } test_cases[]{
      // Default case where there is enough space on all sides.
      // In this case, the popup is placed below meaning that the arrow is on
      // top of the popup.
      {
          views::BubbleArrowSide::kTop,
          gfx::Rect(0, 0, 1000, 2000),
          gfx::Rect(400, 0, 200, 200),
          default_preferred_size,
      },
      // There is enough space on all sides, however the `element_bounds` width
      // is too small and the arrow is therefore placed on the left, as opposed
      // to the top.
      {
          views::BubbleArrowSide::kLeft,
          gfx::Rect(0, 0, 1000, 2000),
          gfx::Rect(400, 0, 1, 200),
          default_preferred_size,
      },
      // There is enough space on all sides, and even though the
      // `element_bounds` width is too small the arrow is still placed on the
      // top. This is because the `anchor_type` is
      // `PopupAnchorType::kCaret`.
      {
          views::BubbleArrowSide::kTop,
          gfx::Rect(0, 0, 1000, 2000),
          gfx::Rect(400, 0, 1, 200),
          default_preferred_size,
          GetDefaultPopupSides(),
          PopupAnchorType::kCaret,
      },
      // Default case where there is enough space on all sides.
      // A different set of the preferred sides.
      {views::BubbleArrowSide::kLeft,
       gfx::Rect(0, 0, 1000, 2000),
       gfx::Rect(400, 0, 200, 200),
       default_preferred_size,
       {views::BubbleArrowSide::kLeft, views::BubbleArrowSide::kRight}},
      // The popup cannot be placed below the element and needs to be placed on
      // top, meaning the arrow is on the bottom of the popup.
      {
          views::BubbleArrowSide::kBottom,
          gfx::Rect(0, 0, 1000, 1000),
          gfx::Rect(0, 800, 200, 200),
          default_preferred_size,
      },
      // There is neither enough space on top nor below the element.
      // It should be placed on the right side meaning the arrow is on the left
      // of the popup.
      {
          views::BubbleArrowSide::kLeft,
          gfx::Rect(0, 0, 1000, 1000),
          gfx::Rect(0, 400, 200, 200),
          default_preferred_size,
      },
      // There is neither enough space on top, below nor on the right side the
      // element. It should be placed on the left side meaning the arrow is on
      // the right of the popup.
      {
          views::BubbleArrowSide::kRight,
          gfx::Rect(0, 0, 1000, 1000),
          gfx::Rect(700, 400, 200, 200),
          default_preferred_size,
      },
      // There is not enough space on any side, but there is more space below
      // the element than above resulting in a placement below with the arrow on
      // top of the popup.
      {
          views::BubbleArrowSide::kTop,
          gfx::Rect(0, 0, 1000, 1000),
          gfx::Rect(0, 100, 200, 200),
          gfx::Size(1200, 1200),
      },
      // There is not enough space on any side, but there is more space above
      // the element than below resulting in a placement above with the arrow
      // below the popup.
      {views::BubbleArrowSide::kBottom, gfx::Rect(0, 0, 1000, 1000),
       gfx::Rect(0, 900, 200, 200), gfx::Size(1200, 1200)},
      // There is enough space, but the preferred sides list is empty,
      // the popup should still be placed with no exceptions.
      {views::BubbleArrowSide::kTop,
       gfx::Rect(0, 0, 1000, 1000),
       gfx::Rect(0, 100, 200, 200),
       gfx::Size(200, 200),
       {}}};

  for (auto& test_case : test_cases) {
    EXPECT_EQ(
        test_case.expected_arrow_side,
        GetOptimalArrowSide(test_case.content_area_bounds,
                            test_case.element_bounds, test_case.preferred_size,
                            test_case.preferred_sides, test_case.anchor_type));
  }
}

TEST(PopupViewsUtilsTest, CalculatePopupBounds) {
  // Define the prompt sizes.
  const int desired_prompt_width = 40;
  const int desired_prompt_height = 30;

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
      {90, 100 - desired_prompt_height},
      {100, 100 - desired_prompt_height},
      // Extreme case: The field is outside of the viewport.
      {120, 100 - desired_prompt_height},
      // Special case: There is not enough space for the desired height.
      {0, 0 + element_height, 0, 30, 30},
      {5, 5 + element_height, 0, 30, 30}};

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
    }
  }
}

TEST(PopupViewsUtilsTest, NotEnoughHeightForAnItem) {
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

TEST(PopupViewsUtilsTest, ElementOutOfContentAreaBounds) {
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

TEST(PopupViewsUtilsTest, GetAvailableVerticalSpaceOnSideOfElement) {
  gfx::Rect content_area_bounds(100, 200, 500, 600);
  gfx::Rect element_bounds(250, 350, 200, 100);

  EXPECT_EQ(
      GetAvailableVerticalSpaceOnSideOfElement(
          content_area_bounds, element_bounds, views::BubbleArrowSide::kLeft),
      content_area_bounds.height());
  EXPECT_EQ(
      GetAvailableVerticalSpaceOnSideOfElement(
          content_area_bounds, element_bounds, views::BubbleArrowSide::kRight),
      content_area_bounds.height());
  // There are 150 pixels above the element and 350 below.
  EXPECT_EQ(
      GetAvailableVerticalSpaceOnSideOfElement(
          content_area_bounds, element_bounds, views::BubbleArrowSide::kTop),
      350);
  EXPECT_EQ(
      GetAvailableVerticalSpaceOnSideOfElement(
          content_area_bounds, element_bounds, views::BubbleArrowSide::kBottom),
      150);
}

TEST(PopupViewsUtilsTest, GetAvailableHorizontalSpaceOnSideOfElement) {
  gfx::Rect content_area_bounds(100, 200, 700, 600);
  gfx::Rect element_bounds(220, 350, 200, 100);

  // The minimum number of pixels the popup should be distanced from the edge
  // of the content area.
  constexpr int kMinimalPopupDistanceToContentAreaEdge = 8;

  EXPECT_EQ(
      GetAvailableHorizontalSpaceOnSideOfElement(
          content_area_bounds, element_bounds, views::BubbleArrowSide::kLeft),
      380 - kMinimalPopupDistanceToContentAreaEdge);
  EXPECT_EQ(
      GetAvailableHorizontalSpaceOnSideOfElement(
          content_area_bounds, element_bounds, views::BubbleArrowSide::kRight),
      120 - kMinimalPopupDistanceToContentAreaEdge);
  EXPECT_EQ(
      GetAvailableHorizontalSpaceOnSideOfElement(
          content_area_bounds, element_bounds, views::BubbleArrowSide::kTop),
      content_area_bounds.width() - 2 * kMinimalPopupDistanceToContentAreaEdge);
  EXPECT_EQ(
      GetAvailableHorizontalSpaceOnSideOfElement(
          content_area_bounds, element_bounds, views::BubbleArrowSide::kBottom),
      content_area_bounds.width() - 2 * kMinimalPopupDistanceToContentAreaEdge);
}

TEST(PopupViewsUtilsTest, IsPopupPlaceableOnSideOfElement) {
  // 200 pixels on top of the element.
  // 55O pixels below the element.
  // 100 pixels on the left side (BubbleArrowSide::kRight)
  // 4OO pixels on the right side.
  gfx::Rect content_area_bounds = {0, 0, 800, 800};
  gfx::Rect element_bounds = {100, 200, 300, 50};
  gfx::Size preferred_size = {200, 300};

  // The popup fits below (BubbleArrowSide::kTop) and on the right side of the
  // element.
  EXPECT_FALSE(IsPopupPlaceableOnSideOfElement(
      content_area_bounds, element_bounds, preferred_size, 12,
      views::BubbleArrowSide::kBottom));
  EXPECT_FALSE(IsPopupPlaceableOnSideOfElement(
      content_area_bounds, element_bounds, preferred_size, 12,
      views::BubbleArrowSide::kRight));
  EXPECT_TRUE(IsPopupPlaceableOnSideOfElement(
      content_area_bounds, element_bounds, preferred_size, 12,
      views::BubbleArrowSide::kLeft));
  EXPECT_TRUE(IsPopupPlaceableOnSideOfElement(
      content_area_bounds, element_bounds, preferred_size, 12,
      views::BubbleArrowSide::kTop));
}

TEST(PopupViewsUtilsTest, GetOptimalPopupArrowSide) {
  // For this test, we fix the content area and the popup size.
  gfx::Rect content_area_bounds = {0, 0, 800, 800};
  gfx::Size preferred_popup_size = {200, 300};

  struct TestCase {
    gfx::Rect element_bounds;
    PopupAnchorType anchor_type;
    views::BubbleArrowSide expected_arrow_side;
  } test_cases[]{
      {gfx::Rect{0, 0, 100, 800}, PopupAnchorType::kField,
       views::BubbleArrowSide::kLeft},
      {gfx::Rect{0, 0, 1, 100}, PopupAnchorType::kField,
       views::BubbleArrowSide::kLeft},
      // PopupAnchorType::kCaret can still have vertical arrows
      // even though their width it small.
      {gfx::Rect{0, 0, 1, 100}, PopupAnchorType::kCaret,
       views::BubbleArrowSide::kTop},
      {gfx::Rect{600, 0, 100, 800}, PopupAnchorType::kField,
       views::BubbleArrowSide::kRight},
      {gfx::Rect{0, 0, 100, 200}, PopupAnchorType::kField,
       views::BubbleArrowSide::kTop},
      {gfx::Rect{0, 600, 100, 200}, PopupAnchorType::kField,
       views::BubbleArrowSide::kBottom},
  };

  for (TestCase& test_case : test_cases) {
    EXPECT_EQ(GetOptimalArrowSide(content_area_bounds, test_case.element_bounds,
                                  preferred_popup_size, GetDefaultPopupSides(),
                                  test_case.anchor_type),
              test_case.expected_arrow_side);
  }
}

TEST(PopupViewsUtilsTest, GetOptimalPopupPlacement) {
  // For this test, the content area bounds and preferred popup size is fixed.
  const gfx::Rect kContentsAreaBounds = {0, 0, 800, 800};
  const gfx::Size kPreferredPopupSize = {200, 300};
  const int kScrollbarWidth = 10;
  const int kMaximumPixelOffsetTowardsCenter = 120;
  const int kMaximumWidthPercentageTowardsCenter = 50;
  const int kHoriztontalPlacementOffsetToAlignArrow =
      views::BubbleBorder::kVisibleArrowBuffer +
      views::BubbleBorder::kVisibleArrowRadius;

  struct TestCase {
    bool right_to_left;
    gfx::Rect element_bounds;
    gfx::Rect expected_popup_bounds;
    PopupAnchorType anchor_type;
    views::BubbleBorder::Arrow expected_arrow;
  } test_cases[]{
      // The element is placed in the top left corner and the popup should be
      // shown
      // below the element is displaced by half the field width.
      {false,
       {0, 0, 100, 20},
       {50 - kHoriztontalPlacementOffsetToAlignArrow, 20, 200, 300},
       PopupAnchorType::kField,
       views::BubbleBorder::Arrow::TOP_LEFT},
      // Because the width of the element is too narrow, the element is placed
      // in the left and the popup should be shown
      // on the right side of the element.
      {false,
       // Note that the width of the `element_bouds` is 1. Which leads to the
       // popup being placed to the
       // left of it.
       {0, 0, 1, 20},
       {1, 0, 200, 300},
       PopupAnchorType::kField,
       views::BubbleBorder::Arrow::LEFT_TOP},
      // Even though the width of the element is too narrow, the element is
      // still placed in the top and the popup should be shown.
      // This because `PopupAnchorType::kCaret` elements
      // are by design narrow.
      {false,
       // Note that the width of the `element_bouds` is 1.
       {0, 0, 1, 20},
       // The 8 matches the `kMinimalPopupDistanceToContentAreaEdge`. Because
       // the width is too small, the popup would be aligned to left of the
       // content area
       // (by using a negative value to x axis offset). However, there is an
       // inner check that does not allow this to happen and make sure that
       // the x coordinate is at least `kMinimalPopupDistanceToContentAreaEdge`.
       {8, 20, 200, 300},
       PopupAnchorType::kCaret,
       views::BubbleBorder::Arrow::TOP_LEFT},
      // The element is placed in the top right corner and the popup needs to
      // be moved back into the view port honoring the minimal distance to the
      // content area edge.
      {false,
       {760, 0, 100, 20},
       {592, 20, 200, 300},
       PopupAnchorType::kField,
       views::BubbleBorder::Arrow::TOP_LEFT},
      // The element is placed in the top corner and the popup should be shown
      // below the element, displaced by maximum of 120 pixels.
      {false,
       {0, 0, 300, 20},
       {kMaximumPixelOffsetTowardsCenter -
            kHoriztontalPlacementOffsetToAlignArrow,
        20, 200, 300},
       PopupAnchorType::kField,

       views::BubbleBorder::Arrow::TOP_LEFT},
      // The element is placed in the lower left corner which should create a
      // popup on top of the element.
      {false,
       {0, 780, 100, 20},
       {50 - kHoriztontalPlacementOffsetToAlignArrow, 480, 200, 300},
       PopupAnchorType::kField,
       views::BubbleBorder::Arrow::BOTTOM_LEFT},
      // Test a basic right website with an element placed on the upper
      // right corner. The popup should be displaced to the left.
      {true,
       {700, 0, 100, 20},
       {550 + kHoriztontalPlacementOffsetToAlignArrow, 20, 200, 300},
       PopupAnchorType::kField,
       views::BubbleBorder::Arrow::TOP_RIGHT},
      // Test a field that is barely visible. This should create a popup on the
      // side.
      {false,
       {-95, 300, 100, 20},
       {5, 300, 200, 300},
       PopupAnchorType::kField,
       views::BubbleBorder::Arrow::LEFT_TOP},
  };

  for (TestCase& test_case : test_cases) {
    gfx::Rect popup_bounds;

    EXPECT_EQ(test_case.expected_arrow,
              GetOptimalPopupPlacement(
                  kContentsAreaBounds, test_case.element_bounds,
                  kPreferredPopupSize, test_case.right_to_left, kScrollbarWidth,
                  kMaximumPixelOffsetTowardsCenter,
                  kMaximumWidthPercentageTowardsCenter, popup_bounds,
                  GetDefaultPopupSides(), test_case.anchor_type));

    EXPECT_EQ(popup_bounds, test_case.expected_popup_bounds);
  }
}

}  // namespace autofill
