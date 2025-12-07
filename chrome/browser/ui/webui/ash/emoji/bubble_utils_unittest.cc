// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/emoji/bubble_utils.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/test/test_screen.h"

using display::test::TestScreen;

using ::testing::TestWithParam;
using ::testing::ValuesIn;

namespace {

struct GetBubbleBoundsAroundCaretTestParams {
  gfx::Rect caret_bounds;
  gfx::Outsets bubble_border_outsets;
  gfx::Size bubble_size;
  gfx::Rect bubble_bounds;
};

// The test cases here are based on test screen (width = 800, height = 600).
std::vector<GetBubbleBoundsAroundCaretTestParams>
    get_bubble_bounds_around_caret_test_params = {
        // When: caret appears on top left corner.
        // Then: bubble appears below caret.
        {
            .caret_bounds = gfx::Rect(0, 0, 100, 20),
            .bubble_border_outsets = gfx::Outsets(8),
            .bubble_size = gfx::Size(404, 464),
            .bubble_bounds = gfx::Rect(16, 20, 420, 480),
        },
        // When: caret appears on top right corner.
        // Then: bubbles appears below caret.
        {
            .caret_bounds = gfx::Rect(700, 0, 100, 20),
            .bubble_border_outsets = gfx::Outsets(8),
            .bubble_size = gfx::Size(404, 464),
            .bubble_bounds = gfx::Rect(364, 20, 420, 480),
        },
        // When: caret appears on bottom left corner.
        // Then: bubbles appears above caret.
        {
            .caret_bounds = gfx::Rect(0, 580, 100, 20),
            .bubble_border_outsets = gfx::Outsets(8),
            .bubble_size = gfx::Size(404, 464),
            .bubble_bounds = gfx::Rect(16, 100, 420, 480),
        },
        // When: caret appears on bottom right corner.
        // Then: bubbles appears above caret.
        {
            .caret_bounds = gfx::Rect(700, 580, 100, 20),
            .bubble_border_outsets = gfx::Outsets(8),
            .bubble_size = gfx::Size(404, 464),
            .bubble_bounds = gfx::Rect(364, 100, 420, 480),
        },
};

class BubbleUtilsTest
    : public TestWithParam<GetBubbleBoundsAroundCaretTestParams> {
 protected:
  TestScreen test_screen_{/*create_display=*/true, /*register_screen=*/true};
};

TEST_P(BubbleUtilsTest, GetBubbleBoundsAroundCaret) {
  gfx::Rect bubble_bounds = ash::GetBubbleBoundsAroundCaret(
      GetParam().caret_bounds, GetParam().bubble_border_outsets,
      GetParam().bubble_size);

  // Check the bubble is positioned as expected.
  EXPECT_EQ(bubble_bounds, GetParam().bubble_bounds);

  // Check the bubble is not displayed off-screen.
  EXPECT_GT(bubble_bounds.x(), 0);
  EXPECT_GT(bubble_bounds.y(), 0);
  EXPECT_LT(bubble_bounds.x() + bubble_bounds.width(),
            TestScreen::kDefaultScreenBounds.width());
  EXPECT_LT(bubble_bounds.y() + bubble_bounds.height(),
            TestScreen::kDefaultScreenBounds.height());
}

INSTANTIATE_TEST_SUITE_P(BubbleUtilsTestAll,
                         BubbleUtilsTest,
                         ValuesIn(get_bubble_bounds_around_caret_test_params));

}  // namespace
