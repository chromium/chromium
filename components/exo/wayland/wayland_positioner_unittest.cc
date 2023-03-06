// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/wayland_positioner.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "xdg-shell-server-protocol.h"

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

    explicit TestCaseBuilder() { positioner.SetAnchorRect({2, 2, 1, 1}); }

    TestCaseBuilder& SetFlipState(bool x, bool y) {
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

    TestCaseBuilder& SetWorkArea(const gfx::Rect& rect) {
      work_area = rect;
      return *this;
    }

    TestCaseBuilder& SetSize(int w, int h) {
      positioner.SetSize({w, h});
      return *this;
    }

    WaylandPositioner::Result Solve() const {
      return positioner.CalculateBounds(work_area);
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
                .SetAnchor(XDG_POSITIONER_ANCHOR_RIGHT)
                .SolveToRect(),
            gfx::Rect(2, 2, 2, 1));
  EXPECT_EQ(TestCaseBuilder()
                .SetSize(2, 1)
                .SetAnchor(XDG_POSITIONER_ANCHOR_LEFT)
                .SolveToRect(),
            gfx::Rect(1, 2, 2, 1));

  // Gravity without anchor.
  EXPECT_EQ(TestCaseBuilder()
                .SetSize(1, 2)
                .SetAnchorRect(2, 2, 0, 0)
                .SetGravity(XDG_POSITIONER_GRAVITY_TOP)
                .SolveToRect(),
            gfx::Rect(2, 0, 1, 2));
  EXPECT_EQ(TestCaseBuilder()
                .SetSize(1, 2)
                .SetAnchorRect(2, 2, 0, 0)
                .SetGravity(XDG_POSITIONER_GRAVITY_BOTTOM)
                .SolveToRect(),
            gfx::Rect(2, 2, 1, 2));

  // Gravity + anchor in the same direction.
  EXPECT_EQ(TestCaseBuilder()
                .SetSize(2, 2)
                .SetGravity(XDG_POSITIONER_GRAVITY_BOTTOM_LEFT)
                .SetAnchor(XDG_POSITIONER_ANCHOR_BOTTOM_LEFT)
                .SolveToRect(),
            gfx::Rect(0, 3, 2, 2));

  // Gravity + anchor in opposing directions.
  EXPECT_EQ(TestCaseBuilder()
                .SetSize(2, 2)
                .SetGravity(XDG_POSITIONER_GRAVITY_BOTTOM_LEFT)
                .SetAnchor(XDG_POSITIONER_ANCHOR_TOP_RIGHT)
                .SolveToRect(),
            gfx::Rect(1, 2, 2, 2));
}

TEST_F(WaylandPositionerTest, FlipSlideResizePriority) {
  TestCaseBuilder builder;
  builder.SetAnchorRect(4, 4, 0, 0)
      .SetSize(2, 2)
      .SetGravity(XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT)
      .SetAnchor(XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT);
  // Flip is enabled, so the result will be at 2,2 (i.e. flipping a 2-wide
  // square around 4,4).
  EXPECT_EQ(builder.SetAdjustment(~XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_NONE)
                .SolveToRect(),
            gfx::Rect(2, 2, 2, 2));
  // If we cant flip on an axis, that axis will slide to 3 instead.
  EXPECT_EQ(builder.SetAdjustment(~XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_X)
                .SolveToRect(),
            gfx::Rect(3, 2, 2, 2));
  EXPECT_EQ(builder.SetAdjustment(~XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y)
                .SolveToRect(),
            gfx::Rect(2, 3, 2, 2));
  // If we cant flip or slide, we resize.
  EXPECT_EQ(builder
                .SetAdjustment(XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_X |
                               XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_Y)
                .SolveToRect(),
            gfx::Rect(4, 4, 1, 1));
}

TEST_F(WaylandPositionerTest, TriesToMaximizeArea) {
  // The size is too large to fit where the anchor is.
  WaylandPositioner::Result result =
      TestCaseBuilder()
          .SetAnchorRect(2, 4, 0, 0)
          .SetSize(4, 10)
          .SetGravity(XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT)
          .SetAnchor(XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT)
          .SetAdjustment(~XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_NONE)
          .Solve();
  // We can slide to 1 on x, but we must resize on y (after sliding to 0).
  EXPECT_EQ(result.origin, gfx::Point(1, 0));
  // The x size will be preserved but y shrinks to the work area.
  EXPECT_EQ(result.size, gfx::Size(4, 5));
}

TEST_F(WaylandPositionerTest, PropagatesAnInitialFlip) {
  WaylandPositioner::Result result =
      TestCaseBuilder()
          .SetAnchorRect(3, 1, 0, 0)
          .SetSize(2, 2)
          .SetGravity(XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT)
          .SetAnchor(XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT)
          .SetAdjustment(~XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_NONE)
          .SetFlipState(true, true)
          .Solve();
  // With a propagated flip state:
  //  - X and Y remain flipped to be positioned by the client.
  EXPECT_EQ(result.origin, gfx::Point(3, 1));
  EXPECT_EQ(result.size, gfx::Size(2, 2));
}

// This is a common case for dropdown menus. In ChromeOS we do not let them
// slide if they might occlude the anchor rectangle. For this case, x axis does
// slide but the y axis resized instead.
TEST_F(WaylandPositionerTest, PreventsSlidingThatOccludesAnchorRect) {
  EXPECT_EQ(TestCaseBuilder()
                .SetSize(3, 3)
                .SetGravity(XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT)
                .SetAnchor(XDG_POSITIONER_ANCHOR_BOTTOM_LEFT)
                .SetAdjustment(~XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_NONE)
                .SolveToRect(),
            gfx::Rect(2, 3, 3, 2));

  // Here we ensure that the 4x4 popup does slide, which is allowed because
  // the anchor rect is already occluded.
  EXPECT_EQ(TestCaseBuilder()
                .SetSize(4, 4)
                .SetGravity(XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT)
                .SetAnchor(XDG_POSITIONER_ANCHOR_TOP_LEFT)
                .SetAdjustment(~XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_NONE)
                .SolveToRect(),
            gfx::Rect(1, 1, 4, 4));
}

// Allowing sliding which will occlude the anchor if there are no other
// positioning options which do not result in a constrained view available.
TEST_F(WaylandPositionerTest,
       AllowsSlidingThatOccludesWhenThereAreNoOtherOptions) {
  EXPECT_EQ(TestCaseBuilder()
                .SetSize(4, 4)
                .SetGravity(XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT)
                .SetAnchor(XDG_POSITIONER_ANCHOR_BOTTOM_LEFT)
                // Disable resizing in both axes which will force sliding.
                .SetAdjustment(~(XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_X |
                                 XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_Y))
                .SolveToRect(),
            gfx::Rect(1, 1, 4, 4));
}

// Make sure that the size should never be an empty even if the constraints
// resulted in empty size.
TEST_F(WaylandPositionerTest, ResizableShouldNotBeEmpty) {
  EXPECT_EQ(TestCaseBuilder()
                .SetSize(3, 3)
                .SetGravity(XDG_POSITIONER_GRAVITY_BOTTOM)
                .SetAnchor(XDG_POSITIONER_ANCHOR_BOTTOM)
                .SetAdjustment(~XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_NONE)
                .SetAnchorRect(1, -10, 4, 4)
                .SolveToRect(),
            gfx::Rect(2, 0, 3, 1));
  EXPECT_EQ(TestCaseBuilder()
                .SetSize(3, 3)
                .SetGravity(XDG_POSITIONER_GRAVITY_RIGHT)
                .SetAnchor(XDG_POSITIONER_ANCHOR_RIGHT)
                .SetAdjustment(~XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_NONE)
                .SetAnchorRect(-10, 2, 4, 4)
                .SolveToRect(),
            gfx::Rect(0, 2, 1, 3));
}

TEST_F(WaylandPositionerTest,
       AllowsAdditionalAdjustmentsIfNoSolutionCanBeFound) {
  EXPECT_EQ(TestCaseBuilder()
                .SetWorkArea(gfx::Rect(5, 5))
                .SetSize(10, 10)
                .SetAnchorRect(0, 0, 0, 0)
                .SetGravity(XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT)
                // No solution should forcibly allow resize
                .SetAdjustment(XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X |
                               XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y)
                .SolveToRect(),
            gfx::Rect(0, 0, 5, 5));
}

}  // namespace
}  // namespace wayland
}  // namespace exo
