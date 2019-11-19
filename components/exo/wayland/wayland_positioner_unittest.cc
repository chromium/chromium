// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/wayland_positioner.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "xdg-shell-unstable-v6-server-protocol.h"

namespace exo {
namespace wayland {
namespace {

class WaylandPositionerTest : public testing::Test {
 protected:
  // By default the test cases happen on a 5x5 grid with an anchor rect at
  // (2,2,1x1).
  struct TestCaseBuilder {
    WaylandPositioner positioner;
    gfx::Rect work_area = {0, 0, 5, 5};
    bool flip_x = false;
    bool flip_y = false;

    TestCaseBuilder() { positioner.SetAnchorRect({2, 2, 1, 1}); }

    TestCaseBuilder& SetFlipState(bool x, bool y) {
      flip_x = x;
      flip_y = y;
      return *this;
    }

    TestCaseBuilder& SetAnchor(uint32_t anchor) {
      positioner.SetAnchor(anchor);
      return *this;
    }

    TestCaseBuilder& SetGravity(uint32_t gravity) {
      positioner.SetGravity(gravity);
      return *this;
    }

    TestCaseBuilder& SetAdjustment(uint32_t adjustment) {
      positioner.SetAdjustment(adjustment);
      return *this;
    }

    TestCaseBuilder& SetAnchorRect(int x, int y, int w, int h) {
      positioner.SetAnchorRect({x, y, w, h});
      return *this;
    }

    TestCaseBuilder& SetSize(uint32_t w, uint32_t h) {
      positioner.SetSize({w, h});
      return *this;
    }

    WaylandPositioner::Result Solve() const {
      return positioner.CalculatePosition(work_area, flip_x, flip_y);
    }

    gfx::Rect SolveToRect() const {
      WaylandPositioner::Result result = Solve();
      return {result.origin.x(), result.origin.y(), result.size.width(),
              result.size.height()};
    }
  };
};

TEST_F(WaylandPositionerTest, UnconstrainedCases) {
  // No gravity or anchor.
  EXPECT_EQ(TestCaseBuilder().SetSize(1, 1).SolveToRect(),
            gfx::Rect(2, 2, 1, 1));

  // Anchor without gravity.
  EXPECT_EQ(TestCaseBuilder()
                .SetSize(2, 1)
                .SetAnchor(ZXDG_POSITIONER_V6_ANCHOR_RIGHT)
                .SolveToRect(),
            gfx::Rect(2, 2, 2, 1));
  EXPECT_EQ(TestCaseBuilder()
                .SetSize(2, 1)
                .SetAnchor(ZXDG_POSITIONER_V6_ANCHOR_LEFT)
                .SolveToRect(),
            gfx::Rect(1, 2, 2, 1));

  // Gravity without anchor.
  EXPECT_EQ(TestCaseBuilder()
                .SetSize(1, 2)
                .SetAnchorRect(2, 2, 0, 0)
                .SetGravity(ZXDG_POSITIONER_V6_GRAVITY_TOP)
                .SolveToRect(),
            gfx::Rect(2, 0, 1, 2));
  EXPECT_EQ(TestCaseBuilder()
                .SetSize(1, 2)
                .SetAnchorRect(2, 2, 0, 0)
                .SetGravity(ZXDG_POSITIONER_V6_GRAVITY_BOTTOM)
                .SolveToRect(),
            gfx::Rect(2, 2, 1, 2));

  // Gravity + anchor in the same direction.
  EXPECT_EQ(TestCaseBuilder()
                .SetSize(2, 2)
                .SetGravity(ZXDG_POSITIONER_V6_GRAVITY_BOTTOM |
                            ZXDG_POSITIONER_V6_GRAVITY_LEFT)
                .SetAnchor(ZXDG_POSITIONER_V6_ANCHOR_BOTTOM |
                           ZXDG_POSITIONER_V6_ANCHOR_LEFT)
                .SolveToRect(),
            gfx::Rect(0, 3, 2, 2));

  // Gravity + anchor in opposing directions.
  EXPECT_EQ(TestCaseBuilder()
                .SetSize(2, 2)
                .SetGravity(ZXDG_POSITIONER_V6_GRAVITY_BOTTOM |
                            ZXDG_POSITIONER_V6_GRAVITY_LEFT)
                .SetAnchor(ZXDG_POSITIONER_V6_ANCHOR_TOP |
                           ZXDG_POSITIONER_V6_ANCHOR_RIGHT)
                .SolveToRect(),
            gfx::Rect(1, 2, 2, 2));
}

TEST_F(WaylandPositionerTest, FlipSlideResizePriority) {
  TestCaseBuilder builder;
  builder.SetAnchorRect(4, 4, 0, 0)
      .SetSize(2, 2)
      .SetGravity(ZXDG_POSITIONER_V6_GRAVITY_BOTTOM |
                  ZXDG_POSITIONER_V6_GRAVITY_RIGHT)
      .SetAnchor(ZXDG_POSITIONER_V6_ANCHOR_BOTTOM |
                 ZXDG_POSITIONER_V6_ANCHOR_RIGHT);
  // Flip is enabled, so the result will be at 2,2 (i.e. flipping a 2-wide
  // square around 4,4).
  EXPECT_EQ(
      builder.SetAdjustment(~ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_NONE)
          .SolveToRect(),
      gfx::Rect(2, 2, 2, 2));
  // If we cant flip on an axis, that axis will slide to 3 instead.
  EXPECT_EQ(
      builder.SetAdjustment(~ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_FLIP_X)
          .SolveToRect(),
      gfx::Rect(3, 2, 2, 2));
  EXPECT_EQ(
      builder.SetAdjustment(~ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_FLIP_Y)
          .SolveToRect(),
      gfx::Rect(2, 3, 2, 2));
  // If we cant flip or slide, we resize.
  EXPECT_EQ(
      builder
          .SetAdjustment(ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_RESIZE_X |
                         ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_RESIZE_Y)
          .SolveToRect(),
      gfx::Rect(4, 4, 1, 1));
}

TEST_F(WaylandPositionerTest, TriesToMaximizeArea) {
  // The size is too large to fit where the anchor is.
  WaylandPositioner::Result result =
      TestCaseBuilder()
          .SetAnchorRect(2, 4, 0, 0)
          .SetSize(4, 10)
          .SetGravity(ZXDG_POSITIONER_V6_GRAVITY_BOTTOM |
                      ZXDG_POSITIONER_V6_GRAVITY_RIGHT)
          .SetAnchor(ZXDG_POSITIONER_V6_ANCHOR_BOTTOM |
                     ZXDG_POSITIONER_V6_ANCHOR_RIGHT)
          .SetAdjustment(~ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_NONE)
          .Solve();
  // We can slide to 1 on x, but we must resize on y (after sliding to 0).
  EXPECT_EQ(result.origin, gfx::Point(1, 0));
  // The x size will be preserved but y shrinks to the work area.
  EXPECT_EQ(result.size, gfx::Size(4, 5));
  // Neither axis will be flipped.
  EXPECT_FALSE(result.x_flipped);
  EXPECT_FALSE(result.y_flipped);
}

TEST_F(WaylandPositionerTest, PropagatesAnInitialFlip) {
  WaylandPositioner::Result result =
      TestCaseBuilder()
          .SetAnchorRect(3, 1, 0, 0)
          .SetSize(2, 2)
          .SetGravity(ZXDG_POSITIONER_V6_GRAVITY_BOTTOM |
                      ZXDG_POSITIONER_V6_GRAVITY_RIGHT)
          .SetAnchor(ZXDG_POSITIONER_V6_ANCHOR_BOTTOM |
                     ZXDG_POSITIONER_V6_ANCHOR_RIGHT)
          .SetAdjustment(~ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_NONE)
          .SetFlipState(true, true)
          .Solve();
  // With a propagated flip state:
  //  - Normally the x would not need to flip, but it propagates the flip.
  //  - Y also propagates, but that makes it constrained so it flips back.
  EXPECT_EQ(result.origin, gfx::Point(1, 1));
  EXPECT_EQ(result.size, gfx::Size(2, 2));
  EXPECT_TRUE(result.x_flipped);
  EXPECT_FALSE(result.y_flipped);
}

// This is a common case for dropdown menus. In ChromeOS we do not let them
// slide if they might occlude the anchor rectangle. For this case, x axis does
// slide but the y axis resized instead.
TEST_F(WaylandPositionerTest, PreventsSlidingThatOccludesAnchorRect) {
  EXPECT_EQ(TestCaseBuilder()
                .SetSize(3, 3)
                .SetGravity(ZXDG_POSITIONER_V6_GRAVITY_BOTTOM |
                            ZXDG_POSITIONER_V6_GRAVITY_RIGHT)
                .SetAnchor(ZXDG_POSITIONER_V6_ANCHOR_BOTTOM |
                           ZXDG_POSITIONER_V6_ANCHOR_LEFT)
                .SetAdjustment(~ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_NONE)
                .SolveToRect(),
            gfx::Rect(2, 3, 3, 2));
}

}  // namespace
}  // namespace wayland
}  // namespace exo
