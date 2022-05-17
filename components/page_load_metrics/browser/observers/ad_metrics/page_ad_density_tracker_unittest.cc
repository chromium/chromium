// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "components/page_load_metrics/browser/observers/ad_metrics/page_ad_density_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace page_load_metrics {

TEST(PageAdDensityTrackerTest, MultipleRects_MaxPageDensityByAreaCalculated) {
  PageAdDensityTracker tracker;

  // Page ad density is -1 before there is a main frame or subframes.
  EXPECT_EQ(tracker.MaxPageAdDensityByArea(), -1);

  tracker.UpdateMainFrameRect(gfx::Rect(0, 0, 100, 100));
  tracker.AddRect(1 /* rect_id */, gfx::Rect(0, 0, 100, 10));
  EXPECT_EQ(tracker.MaxPageAdDensityByArea(), 10);

  tracker.AddRect(2 /* rect_id */, gfx::Rect(5, 5, 100, 10));
  EXPECT_EQ(tracker.MaxPageAdDensityByArea(), 15);

  tracker.AddRect(3 /* rect_id */, gfx::Rect(50, 50, 50, 50));
  EXPECT_EQ(tracker.MaxPageAdDensityByArea(), 40);

  // Removing a rect should not change the maximum ad density.
  tracker.RemoveRect(3 /* rect_id */);
  EXPECT_EQ(tracker.MaxPageAdDensityByArea(), 40);
}

TEST(PageAdDensityTrackerTest, MultipleRects_MaxPageDensityByHeightCalculated) {
  PageAdDensityTracker tracker;

  // Page ad density is -1 before there is a main frame or subframes.
  EXPECT_EQ(tracker.MaxPageAdDensityByHeight(), -1);

  tracker.UpdateMainFrameRect(gfx::Rect(0, 0, 100, 100));
  tracker.AddRect(1 /* rect_id */, gfx::Rect(0, 0, 100, 10));
  EXPECT_EQ(tracker.MaxPageAdDensityByHeight(), 10);

  tracker.AddRect(2 /* rect_id */, gfx::Rect(5, 5, 100, 10));
  EXPECT_EQ(tracker.MaxPageAdDensityByHeight(), 15);

  tracker.AddRect(3 /* rect_id */, gfx::Rect(50, 50, 50, 50));
  EXPECT_EQ(tracker.MaxPageAdDensityByHeight(), 65);

  // Removing a rect should not change the maximum ad density.
  tracker.RemoveRect(3 /* rect_id */);
  EXPECT_EQ(tracker.MaxPageAdDensityByHeight(), 65);
}

// Remove a rect that was added twice, the second RemoveRect is
// ignored as it is no longer being tracked.
TEST(PageAdDensityTrackerTest, RemoveRectTwice_SecondRemoveIgnored) {
  PageAdDensityTracker tracker;

  tracker.AddRect(1 /* rect_id */, gfx::Rect(0, 0, 100, 10));
  tracker.RemoveRect(1 /* rect_id */);
  tracker.RemoveRect(1 /* rect_id */);
}

// Ensures that two rects with the same dimensions hash to different
// values in the density tracker's frame set.
TEST(PageAdDensityTrackerTest, SeperateRects_SameDimensions) {
  PageAdDensityTracker tracker;

  tracker.UpdateMainFrameRect(gfx::Rect(0, 0, 100, 100));

  tracker.AddRect(1 /* rect_id */, gfx::Rect(0, 0, 100, 10));
  tracker.AddRect(2 /* rect_id */, gfx::Rect(0, 0, 100, 10));
  EXPECT_EQ(tracker.MaxPageAdDensityByArea(), 10);

  tracker.RemoveRect(1 /* rect_id */);
  tracker.RemoveRect(2 /* rect_id */);
  EXPECT_EQ(tracker.MaxPageAdDensityByArea(), 10);
}

// Create 2 rects whose total area overflow an int.
TEST(PageAdDensityTrackerTest, OverflowTotalAreaAndHeight) {
  PageAdDensityTracker tracker;

  tracker.AddRect(1 /* rect_id */, gfx::Rect(std::numeric_limits<int>::min(), 0,
                                             std::numeric_limits<int>::max(),
                                             std::numeric_limits<int>::max()));
  tracker.AddRect(2 /* rect_id */, gfx::Rect(std::numeric_limits<int>::min(),
                                             std::numeric_limits<int>::max(),
                                             std::numeric_limits<int>::max(),
                                             std::numeric_limits<int>::max()));

  // Update main frame rect to force a calculation.
  tracker.UpdateMainFrameRect(gfx::Rect(0, 0, 100, 100));

  // Density should not be updated as the sum of area
  // or height overflows.
  EXPECT_EQ(tracker.MaxPageAdDensityByArea(), -1);
  EXPECT_EQ(tracker.MaxPageAdDensityByHeight(), -1);
}

// Regression test for crbug.com/1241038 (i.e. potential DCHECK failure if a
// line segment starts at position -1).
TEST(PageAdDensityTrackerTest, RectAtSpecialPosition) {
  PageAdDensityTracker tracker;

  tracker.UpdateMainFrameRect(gfx::Rect(0, 0, 100, 100));

  tracker.AddRect(1 /* rect_id */, gfx::Rect(-1, -1, 1, 1));

  EXPECT_EQ(tracker.MaxPageAdDensityByArea(), 0);
  EXPECT_EQ(tracker.MaxPageAdDensityByHeight(), 1);
}

}  // namespace page_load_metrics
