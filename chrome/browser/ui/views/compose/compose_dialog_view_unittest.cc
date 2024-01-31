// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/compose/compose_dialog_view.h"

#include "components/compose/core/browser/config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::string PositioningStrategyToString(
    ::testing::TestParamInfo<compose::DialogFallbackPositioningStrategy>
        strategy) {
  switch (strategy.param) {
    case compose::DialogFallbackPositioningStrategy::
        kShiftUpUntilMaxSizeIsOnscreen:
      return "kShiftUpUntilMaxSizeIsOnscreen";
    case compose::DialogFallbackPositioningStrategy::kCenterOnAnchorRect:
      return "kCenterOnAnchorRect";
    case compose::DialogFallbackPositioningStrategy::kShiftUpUntilOnscreen:
      return "kShiftUpUntilOnscreen";
    default:
      return "InvalidStrategy";
  }
}

gfx::Size DefaultWidgetSize() {
  return gfx::Size(300, 200);
}
gfx::Rect DefaultScreenWorkArea() {
  return gfx::Rect(0, 0, 1000, 1000);
}
gfx::Size DefaultAnchorSize() {
  return gfx::Size(400, 400);
}

}  // namespace

class ComposeDialogViewTest : public testing::TestWithParam<
                                  compose::DialogFallbackPositioningStrategy> {
  void SetUp() override {
    compose::GetMutableConfigForTesting().positioning_strategy = GetParam();
  }

  void TearDown() override { compose::ResetConfigForTesting(); }
};

TEST_P(ComposeDialogViewTest, TestLayoutBelow) {
  // Set up params such that the compose dialog will fit in the optimal position
  // directly below and left aligned with the anchor.
  gfx::Rect anchor_bounds{{100, 100}, DefaultAnchorSize()};

  gfx::Rect bounds = ComposeDialogView::CalculateBubbleBounds(
      DefaultScreenWorkArea(), DefaultWidgetSize(), anchor_bounds);

  // Must be onscreen.
  EXPECT_TRUE(DefaultScreenWorkArea().Contains(bounds));
  // Must not change the size
  EXPECT_EQ(bounds.size(), DefaultWidgetSize());

  // Doesn't matter which param in this case, since this is not a fallback.
  // Assert that we are arranged below the anchor,
  EXPECT_EQ(anchor_bounds.bottom(),
            bounds.y() + ComposeDialogView::kComposeDialogAnchorPadding);
}

TEST_P(ComposeDialogViewTest, TestLayoutAbove) {
  // Set up params such that the compose dialog will fit in the optimal position
  // directly below and left aligned with the anchor.
  gfx::Rect anchor_bounds{{100, 500}, DefaultAnchorSize()};

  gfx::Rect bounds = ComposeDialogView::CalculateBubbleBounds(
      DefaultScreenWorkArea(), DefaultWidgetSize(), anchor_bounds);

  // Must be onscreen.
  EXPECT_TRUE(DefaultScreenWorkArea().Contains(bounds));
  // Must not change the size
  EXPECT_EQ(bounds.size(), DefaultWidgetSize());

  // Doesn't matter which param in this case, since this is not a fallback.
  // Assert that we are arranged above the anchor,
  EXPECT_EQ(anchor_bounds.y(),
            bounds.bottom() + ComposeDialogView::kComposeDialogAnchorPadding);
}

TEST_P(ComposeDialogViewTest, TestAnchorOnRight) {
  gfx::Rect anchor_bounds({800, 100}, DefaultAnchorSize());
  gfx::Rect bounds = ComposeDialogView::CalculateBubbleBounds(
      DefaultScreenWorkArea(), DefaultWidgetSize(), anchor_bounds);

  // Must be onscreen. In this case, that means that the bounds rect will be
  // shifted to the left to remain onscreen..
  EXPECT_TRUE(DefaultScreenWorkArea().Contains(bounds));
  // Must not change the size
  EXPECT_EQ(bounds.size(), DefaultWidgetSize());

  // Doesn't matter which param in this case, since this is not a fallback.
  // Assert that we are arranged below the anchor,
  EXPECT_EQ(anchor_bounds.bottom(),
            bounds.y() + ComposeDialogView::kComposeDialogAnchorPadding);
}

TEST_P(ComposeDialogViewTest, TestAnchorOnLeft) {
  gfx::Rect anchor_bounds({-100, 100}, DefaultAnchorSize());
  gfx::Rect bounds = ComposeDialogView::CalculateBubbleBounds(
      DefaultScreenWorkArea(), DefaultWidgetSize(), anchor_bounds);

  // Must be onscreen. In this case, that means that the bounds rect will be
  // shifted to the left to remain onscreen..
  EXPECT_TRUE(DefaultScreenWorkArea().Contains(bounds));
  // Must not change the size
  EXPECT_EQ(bounds.size(), DefaultWidgetSize());

  // Doesn't matter which param in this case, since this is not a fallback.
  // Assert that we are arranged below the anchor,
  EXPECT_EQ(anchor_bounds.bottom(),
            bounds.y() + ComposeDialogView::kComposeDialogAnchorPadding);
}

TEST_P(ComposeDialogViewTest, TestFallbackVertical) {
  // Too big to fit the dialog entirely on any side.
  gfx::Rect anchor_bounds{{100, 100}, gfx::Size(800, 800)};

  gfx::Rect bounds = ComposeDialogView::CalculateBubbleBounds(
      DefaultScreenWorkArea(), DefaultWidgetSize(), anchor_bounds);

  switch (GetParam()) {
    case compose::DialogFallbackPositioningStrategy::kCenterOnAnchorRect:
      EXPECT_EQ(bounds.CenterPoint(), anchor_bounds.CenterPoint());
      break;
    case compose::DialogFallbackPositioningStrategy::kShiftUpUntilOnscreen:
      // Must be |padding| away from the bottom of the screen.
      EXPECT_EQ(bounds.bottom(),
                DefaultScreenWorkArea().bottom() -
                    ComposeDialogView::kComposeDialogWorkAreaPadding);
      break;
    case compose::DialogFallbackPositioningStrategy::
        kShiftUpUntilMaxSizeIsOnscreen:
      // fallthrough - invalid should be the same as this.
    default:
      // Must be at least |padding| away from the bottom of the screen.
      EXPECT_LT(bounds.bottom(),
                DefaultScreenWorkArea().bottom() -
                    ComposeDialogView::kComposeDialogWorkAreaPadding);
      // Should always be rendered a fixed position from the work area bottom
      // (since max height is fixed).
      EXPECT_EQ(bounds.y(),
                DefaultScreenWorkArea().bottom() -
                    ComposeDialogView::kComposeDialogWorkAreaPadding -
                    ComposeDialogView::kComposeMaxDialogHeightPx);
  }
}

INSTANTIATE_TEST_SUITE_P(
    FallbackPositioningStrategy,
    ComposeDialogViewTest,
    testing::Values(
        compose::DialogFallbackPositioningStrategy::kCenterOnAnchorRect,
        compose::DialogFallbackPositioningStrategy::kShiftUpUntilOnscreen,
        compose::DialogFallbackPositioningStrategy::
            kShiftUpUntilMaxSizeIsOnscreen,
        // 999 should behave like the default
        static_cast<compose::DialogFallbackPositioningStrategy>(999)),
    &PositioningStrategyToString);
