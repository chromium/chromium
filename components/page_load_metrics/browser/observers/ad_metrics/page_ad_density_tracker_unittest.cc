// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/ad_metrics/page_ad_density_tracker.h"

#include <limits>

#include "base/containers/flat_map.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace page_load_metrics {

namespace {

using RectId = PageAdDensityTracker::RectId;

const RectId kRectId1 = 1;
const RectId kRectId2 = 2;
const RectId kRectId3 = 3;

}  // namespace

// Only for test purpose.
class PageAdDensityTrackerTestPeer {
 public:
  static bool RectExistsAndHasCorrectTopIterator(
      const PageAdDensityTracker& tracker,
      RectId rect_id) {
    auto it = tracker.rect_events_iterators_.find(rect_id);
    // Rect not exists.
    if (it == tracker.rect_events_iterators_.end())
      return false;
    const PageAdDensityTracker::RectEventSetIterators& set_its = it->second;
    return !set_its.top_it->is_bottom;
  }

  static bool RectExistsAndHasCorrectBottomIterator(
      const PageAdDensityTracker& tracker,
      RectId rect_id) {
    auto it = tracker.rect_events_iterators_.find(rect_id);
    // Rect not exists.
    if (it == tracker.rect_events_iterators_.end())
      return false;
    const PageAdDensityTracker::RectEventSetIterators& set_its = it->second;
    return set_its.bottom_it->is_bottom;
  }
};

TEST(PageAdDensityTrackerTest, MultipleRects_MaxDensity) {
  PageAdDensityTracker tracker(/*is_in_foreground=*/true);

  // Page ad density is -1 before there is a main frame or subframes.
  EXPECT_EQ(tracker.MaxPageAdDensityByArea(), -1);

  tracker.UpdateMainFrameRect(gfx::Rect(0, 0, 100, 100));
  tracker.UpdateMainFrameAdRects({{kRectId1, gfx::Rect(0, 0, 100, 10)}});
  EXPECT_EQ(tracker.MaxPageAdDensityByArea(), 10);
  EXPECT_EQ(tracker.MaxPageAdDensityByHeight(), 10);

  tracker.UpdateMainFrameAdRects({{kRectId2, gfx::Rect(50, 0, 100, 20)}});
  EXPECT_EQ(tracker.MaxPageAdDensityByArea(), 15);
  EXPECT_EQ(tracker.MaxPageAdDensityByHeight(), 20);

  tracker.UpdateMainFrameAdRects({{kRectId3, gfx::Rect(50, 50, 50, 50)}});
  EXPECT_EQ(tracker.MaxPageAdDensityByArea(), 40);
  EXPECT_EQ(tracker.MaxPageAdDensityByHeight(), 70);

  // Removing a rect should not change the maximum ad density.
  tracker.UpdateMainFrameAdRects({{kRectId3, gfx::Rect()}});
  EXPECT_EQ(tracker.MaxPageAdDensityByArea(), 40);
  EXPECT_EQ(tracker.MaxPageAdDensityByHeight(), 70);
}

// Remove a rect that was added twice, the second RemoveRect is
// ignored as it is no longer being tracked.
TEST(PageAdDensityTrackerTest, RemoveRectTwice_SecondRemoveIgnored) {
  PageAdDensityTracker tracker(/*is_in_foreground=*/true);

  tracker.UpdateMainFrameAdRects({{kRectId1, gfx::Rect(0, 0, 100, 10)}});
  tracker.UpdateMainFrameAdRects({{kRectId1, gfx::Rect()}});
  tracker.UpdateMainFrameAdRects({{kRectId1, gfx::Rect()}});
}

// Ensures that two rects with the same dimensions hash to different
// values in the density tracker's frame set.
TEST(PageAdDensityTrackerTest, SeperateRects_SameDimensions) {
  PageAdDensityTracker tracker(/*is_in_foreground=*/true);

  tracker.UpdateMainFrameRect(gfx::Rect(0, 0, 100, 100));

  tracker.UpdateMainFrameAdRects({{kRectId1, gfx::Rect(0, 0, 100, 10)}});
  tracker.UpdateMainFrameAdRects({{kRectId2, gfx::Rect(0, 0, 100, 10)}});
  EXPECT_EQ(tracker.MaxPageAdDensityByArea(), 10);

  tracker.UpdateMainFrameAdRects({{kRectId1, gfx::Rect()}});
  tracker.UpdateMainFrameAdRects({{kRectId2, gfx::Rect()}});
  EXPECT_EQ(tracker.MaxPageAdDensityByArea(), 10);
}

// Create 2 rects whose total area overflow an int.
TEST(PageAdDensityTrackerTest, TwoRectsOverflowTotalAreaAndHeight) {
  PageAdDensityTracker tracker(/*is_in_foreground=*/true);

  tracker.UpdateMainFrameAdRects({
      {kRectId1, gfx::Rect(std::numeric_limits<int>::min(), 0,
                           std::numeric_limits<int>::max(),
                           std::numeric_limits<int>::max())},
      {kRectId2, gfx::Rect(std::numeric_limits<int>::min(),
                           std::numeric_limits<int>::max(),
                           std::numeric_limits<int>::max(),
                           std::numeric_limits<int>::max())},
  });

  // Update main frame rect to force a calculation.
  tracker.UpdateMainFrameRect(gfx::Rect(0, 0, 100, 100));

  // Density should be 0, as there's no intersected area.
  EXPECT_EQ(tracker.MaxPageAdDensityByArea(), 0);
  EXPECT_EQ(tracker.MaxPageAdDensityByHeight(), 0);
}

// Add a main frame rect whose area overflow an int.
TEST(PageAdDensityTrackerTest, OverflowTotalAreaAndHeight) {
  PageAdDensityTracker tracker(/*is_in_foreground=*/true);

  tracker.UpdateMainFrameAdRects(
      {{kRectId1, gfx::Rect(0, 0, std::numeric_limits<int>::max(),
                            std::numeric_limits<int>::max())}});

  // Update main frame rect to force a calculation.
  tracker.UpdateMainFrameRect(gfx::Rect(0, 0, std::numeric_limits<int>::max(),
                                        std::numeric_limits<int>::max()));

  // Density should not be updated as the sum of area or height overflows.
  EXPECT_EQ(tracker.MaxPageAdDensityByArea(), -1);
  EXPECT_EQ(tracker.MaxPageAdDensityByHeight(), -1);
}

// Regression test for crbug.com/1241038 (i.e. potential DCHECK failure if a
// line segment starts at position -1).
TEST(PageAdDensityTrackerTest, RectAtSpecialPosition) {
  PageAdDensityTracker tracker(/*is_in_foreground=*/true);

  tracker.UpdateMainFrameRect(gfx::Rect(0, 0, 100, 100));

  tracker.UpdateMainFrameAdRects({{kRectId1, gfx::Rect(-1, -1, 1, 1)}});

  EXPECT_EQ(tracker.MaxPageAdDensityByArea(), 0);
  EXPECT_EQ(tracker.MaxPageAdDensityByHeight(), 0);
}

// Add a viewport rect whose area overflow an int.
TEST(PageAdDensityTrackerTest, ViewportAdDensity_OverflowViewportArea) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  PageAdDensityTracker tracker(/*is_in_foreground=*/true);

  tracker.UpdateMainFrameAdRects(
      {{kRectId1, gfx::Rect(0, 0, std::numeric_limits<int>::max(),
                            std::numeric_limits<int>::max())}});

  tracker.UpdateMainFrameViewportRect(gfx::Rect(
      0, 0, std::numeric_limits<int>::max(), std::numeric_limits<int>::max()));

  task_environment.FastForwardBy(base::Seconds(1));
  tracker.Finalize();

  // Density should not be updated as the sum of area overflows.
  EXPECT_DOUBLE_EQ(tracker.GetViewportAdDensityByAreaStats().mean, 0);
}

TEST(PageAdDensityTrackerTest, ViewportAdDensity_RectSameSize) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  PageAdDensityTracker tracker(/*is_in_foreground=*/true);

  tracker.UpdateMainFrameViewportRect(gfx::Rect(0, 0, 100, 100));
  tracker.UpdateMainFrameAdRects({{kRectId1, gfx::Rect(0, 0, 100, 100)}});

  task_environment.FastForwardBy(base::Seconds(1));
  tracker.Finalize();
  EXPECT_DOUBLE_EQ(tracker.GetViewportAdDensityByAreaStats().mean, 100);
}

TEST(PageAdDensityTrackerTest, ViewportAdDensity_RectHalfSize) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  PageAdDensityTracker tracker(/*is_in_foreground=*/true);

  tracker.UpdateMainFrameViewportRect(gfx::Rect(0, 0, 100, 100));
  tracker.UpdateMainFrameAdRects({{kRectId1, gfx::Rect(0, 0, 50, 100)}});

  task_environment.FastForwardBy(base::Seconds(1));
  tracker.Finalize();
  EXPECT_DOUBLE_EQ(tracker.GetViewportAdDensityByAreaStats().mean, 50);
}

TEST(PageAdDensityTrackerTest, ViewportAdDensity_RectOutOfViewport) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  PageAdDensityTracker tracker(/*is_in_foreground=*/true);

  tracker.UpdateMainFrameViewportRect(gfx::Rect(0, 0, 100, 100));
  tracker.UpdateMainFrameAdRects({{kRectId1, gfx::Rect(100, 0, 100, 100)}});

  task_environment.FastForwardBy(base::Seconds(1));
  tracker.Finalize();
  EXPECT_DOUBLE_EQ(tracker.GetViewportAdDensityByAreaStats().mean, 0);
}

TEST(PageAdDensityTrackerTest, ViewportAdDensity_RectClipsViewport) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  PageAdDensityTracker tracker(/*is_in_foreground=*/true);

  tracker.UpdateMainFrameViewportRect(gfx::Rect(0, 0, 100, 100));
  tracker.UpdateMainFrameAdRects({{kRectId1, gfx::Rect(50, 50, 100, 100)}});

  task_environment.FastForwardBy(base::Seconds(1));
  tracker.Finalize();
  EXPECT_DOUBLE_EQ(tracker.GetViewportAdDensityByAreaStats().mean, 25);
}

TEST(PageAdDensityTrackerTest, ViewportAdDensity_TwoRectsClipViewport) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  PageAdDensityTracker tracker(/*is_in_foreground=*/true);

  tracker.UpdateMainFrameViewportRect(gfx::Rect(0, 0, 100, 100));
  tracker.UpdateMainFrameAdRects({
      {kRectId1, gfx::Rect(30, 70, 100, 100)},
      {kRectId2, gfx::Rect(70, 30, 100, 100)},
  });

  task_environment.FastForwardBy(base::Seconds(1));
  tracker.Finalize();
  EXPECT_DOUBLE_EQ(tracker.GetViewportAdDensityByAreaStats().mean, 33);
}

TEST(PageAdDensityTrackerTest,
     AverageViewportAdDensity_NoTimeLapseSincePageLoad) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  PageAdDensityTracker tracker(/*is_in_foreground=*/true);

  tracker.UpdateMainFrameViewportRect(gfx::Rect(0, 0, 100, 100));
  tracker.UpdateMainFrameAdRects({{kRectId1, gfx::Rect(0, 0, 50, 50)}});

  tracker.Finalize();
  EXPECT_DOUBLE_EQ(tracker.GetViewportAdDensityByAreaStats().mean, 0);
}

TEST(PageAdDensityTrackerTest, AverageViewportAdDensity_NoViewportRectUpdate) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  PageAdDensityTracker tracker(/*is_in_foreground=*/true);

  tracker.UpdateMainFrameAdRects({{kRectId1, gfx::Rect(0, 0, 50, 50)}});
  task_environment.FastForwardBy(base::Seconds(1));

  tracker.Finalize();
  EXPECT_DOUBLE_EQ(tracker.GetViewportAdDensityByAreaStats().mean, 0);
}

TEST(PageAdDensityTrackerTest, AverageViewportAdDensity_NoAdRectUpdate) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  PageAdDensityTracker tracker(/*is_in_foreground=*/true);

  tracker.UpdateMainFrameViewportRect(gfx::Rect(0, 0, 100, 100));
  task_environment.FastForwardBy(base::Seconds(1));

  tracker.Finalize();
  EXPECT_DOUBLE_EQ(tracker.GetViewportAdDensityByAreaStats().mean, 0);
}

TEST(PageAdDensityTrackerTest,
     AverageViewportAdDensity_CalculateInOneSecondAndImmediateQuery) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  PageAdDensityTracker tracker(/*is_in_foreground=*/true);

  task_environment.FastForwardBy(base::Seconds(1));

  tracker.UpdateMainFrameViewportRect(gfx::Rect(0, 0, 50, 50));
  tracker.UpdateMainFrameAdRects({{kRectId1, gfx::Rect(0, 0, 50, 50)}});

  tracker.Finalize();
  EXPECT_DOUBLE_EQ(tracker.GetViewportAdDensityByAreaStats().mean, 0);
}

TEST(PageAdDensityTrackerTest,
     AverageViewportAdDensity_CalculateInOneSecondAndQueryLater) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  PageAdDensityTracker tracker(/*is_in_foreground=*/true);

  task_environment.FastForwardBy(base::Seconds(1));

  tracker.UpdateMainFrameViewportRect(gfx::Rect(0, 0, 50, 50));
  tracker.UpdateMainFrameAdRects({{kRectId1, gfx::Rect(0, 0, 50, 50)}});

  task_environment.FastForwardBy(base::Seconds(1));

  tracker.Finalize();
  EXPECT_DOUBLE_EQ(tracker.GetViewportAdDensityByAreaStats().mean, 50);
}

TEST(PageAdDensityTrackerTest,
     AverageViewportAdDensity_ViewportRect_SizeUpdate) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  PageAdDensityTracker tracker(/*is_in_foreground=*/true);

  tracker.UpdateMainFrameViewportRect(gfx::Rect(0, 0, 50, 100));
  tracker.UpdateMainFrameAdRects({{kRectId1, gfx::Rect(0, 0, 50, 50)}});

  task_environment.FastForwardBy(base::Seconds(1));

  tracker.UpdateMainFrameViewportRect(gfx::Rect(0, 0, 50, 50));

  task_environment.FastForwardBy(base::Seconds(1));
  tracker.Finalize();
  EXPECT_DOUBLE_EQ(tracker.GetViewportAdDensityByAreaStats().mean, 75);
}

TEST(PageAdDensityTrackerTest,
     AverageViewportAdDensity_ViewportRect_OffsetUpdate) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  PageAdDensityTracker tracker(/*is_in_foreground=*/true);

  tracker.UpdateMainFrameAdRects({{kRectId1, gfx::Rect(0, 0, 50, 50)}});
  tracker.UpdateMainFrameViewportRect(gfx::Rect(0, 0, 50, 50));

  task_environment.FastForwardBy(base::Seconds(1));

  tracker.UpdateMainFrameViewportRect(gfx::Rect(50, 50, 50, 50));

  task_environment.FastForwardBy(base::Seconds(1));
  tracker.Finalize();
  EXPECT_DOUBLE_EQ(tracker.GetViewportAdDensityByAreaStats().mean, 50);
}

TEST(PageAdDensityTrackerTest, AverageViewportAdDensity_AdRectUpdate) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  PageAdDensityTracker tracker(/*is_in_foreground=*/true);

  tracker.UpdateMainFrameAdRects({{kRectId1, gfx::Rect(0, 0, 50, 50)}});
  tracker.UpdateMainFrameViewportRect(gfx::Rect(0, 0, 50, 100));

  task_environment.FastForwardBy(base::Seconds(1));

  tracker.UpdateMainFrameAdRects({{kRectId1, gfx::Rect(0, 0, 50, 100)}});

  task_environment.FastForwardBy(base::Seconds(1));
  tracker.Finalize();
  EXPECT_DOUBLE_EQ(tracker.GetViewportAdDensityByAreaStats().mean, 75);
}

TEST(PageAdDensityTrackerTest,
     AverageViewportAdDensity_RectRemovedAndRecalculateDensity) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  PageAdDensityTracker tracker(/*is_in_foreground=*/true);

  tracker.UpdateMainFrameAdRects({{kRectId1, gfx::Rect(0, 0, 50, 50)}});
  tracker.UpdateMainFrameViewportRect(gfx::Rect(0, 0, 50, 100));

  task_environment.FastForwardBy(base::Seconds(1));

  tracker.UpdateMainFrameAdRects({{kRectId1, gfx::Rect()}});

  task_environment.FastForwardBy(base::Seconds(1));
  tracker.Finalize();

  EXPECT_DOUBLE_EQ(tracker.GetViewportAdDensityByAreaStats().mean, 25);
}

TEST(PageAdDensityTrackerTest,
     AverageViewportAdDensity_MultipleUnequalTimePeriods) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  PageAdDensityTracker tracker(/*is_in_foreground=*/true);

  tracker.UpdateMainFrameAdRects({{kRectId1, gfx::Rect(0, 0, 50, 50)}});
  tracker.UpdateMainFrameViewportRect(gfx::Rect(0, 0, 50, 100));

  task_environment.FastForwardBy(base::Seconds(1));

  tracker.UpdateMainFrameViewportRect(gfx::Rect(25, 0, 50, 100));

  task_environment.FastForwardBy(base::Seconds(2));

  tracker.UpdateMainFrameViewportRect(gfx::Rect(50, 0, 50, 100));

  task_environment.FastForwardBy(base::Seconds(3));
  tracker.Finalize();
  EXPECT_DOUBLE_EQ(tracker.GetViewportAdDensityByAreaStats().mean,
                   (50 * 1 + 25 * 2) / 6.0);
}

TEST(PageAdDensityTrackerTest,
     AverageViewportAdDensity_MultipleRectsInViewportRect) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  PageAdDensityTracker tracker(/*is_in_foreground=*/true);

  tracker.UpdateMainFrameViewportRect(gfx::Rect(50, 0, 50, 100));

  tracker.UpdateMainFrameAdRects({
      // Rect(1) is not within the viewport.
      {kRectId1, gfx::Rect(0, 0, 50, 50)},
      // Rect(2) occupy 1/4 of the viewport.
      {kRectId2, gfx::Rect(25, 0, 50, 50)},
      // Rect(3) occupy 1/4 of the viewport; 1/8 of the viewport is occupied by
      // both Rect(2) and Rect(3)
      {kRectId3, gfx::Rect(25, 25, 50, 50)},
  });

  task_environment.FastForwardBy(base::Seconds(1));
  tracker.Finalize();
  EXPECT_DOUBLE_EQ(tracker.GetViewportAdDensityByAreaStats().mean,
                   int(3 * 100 / 8));
}

TEST(PageAdDensityTrackerTest, RectEvent_CheckTopAndBottomIterator) {
  PageAdDensityTracker tracker(/*is_in_foreground=*/true);
  tracker.UpdateMainFrameAdRects({{kRectId1, gfx::Rect(0, 0, 50, 10)}});

  EXPECT_TRUE(PageAdDensityTrackerTestPeer::RectExistsAndHasCorrectTopIterator(
      tracker, kRectId1));
  EXPECT_TRUE(
      PageAdDensityTrackerTestPeer::RectExistsAndHasCorrectBottomIterator(
          tracker, kRectId1));
  tracker.UpdateMainFrameAdRects({{kRectId1, gfx::Rect()}});
}

TEST(PageAdDensityTrackerTest, AverageViewportAdDensity_OnHiddenAndShown) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  PageAdDensityTracker tracker(/*is_in_foreground=*/true);

  tracker.UpdateMainFrameViewportRect(gfx::Rect(0, 0, 100, 100));
  tracker.UpdateMainFrameAdRects({{kRectId1, gfx::Rect(0, 0, 50, 100)}});

  task_environment.FastForwardBy(base::Seconds(1));

  // Go hidden. Stats for the first second should be accumulated.
  tracker.OnHidden();

  // Stay hidden for 2s
  task_environment.FastForwardBy(base::Seconds(2));

  // Go visible again. Density should be recalculated.
  tracker.OnShown();
  task_environment.FastForwardBy(base::Seconds(1));

  tracker.Finalize();

  // Total visible time = 2s
  // (50% * 1s) + (50% * 1s) = 100
  // Average = 100 / 2.0 = 50.0
  EXPECT_DOUBLE_EQ(tracker.GetViewportAdDensityByAreaStats().mean, 50.0);
}

TEST(PageAdDensityTrackerTest,
     AverageViewportAdDensity_AdRectUpdateWhileHidden) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  PageAdDensityTracker tracker(/*is_in_foreground=*/true);

  tracker.UpdateMainFrameViewportRect(gfx::Rect(0, 0, 100, 100));
  tracker.UpdateMainFrameAdRects({{kRectId1, gfx::Rect(0, 0, 80, 100)}});

  task_environment.FastForwardBy(base::Seconds(1));

  // Go hidden.
  tracker.OnHidden();

  // Update ad rect while hidden.
  tracker.UpdateMainFrameAdRects({{kRectId1, gfx::Rect(0, 0, 20, 100)}});

  // Stay hidden for 1s
  task_environment.FastForwardBy(base::Seconds(1));

  // Go visible. Density should be recalculated.
  tracker.OnShown();
  task_environment.FastForwardBy(base::Seconds(2));

  tracker.Finalize();

  // Total visible time = 3s
  // (80% * 1s) + (20% * 2s) = 80 + 40 = 120
  // Average = 120 / 3.0 = 40.0
  EXPECT_DOUBLE_EQ(tracker.GetViewportAdDensityByAreaStats().mean, 40.0);
}

TEST(PageAdDensityTrackerTest, MaxPageAdDensity_OnHiddenAndShown) {
  PageAdDensityTracker tracker(/*is_in_foreground=*/true);

  tracker.UpdateMainFrameRect(gfx::Rect(0, 0, 100, 100));
  tracker.UpdateMainFrameAdRects({{kRectId1, gfx::Rect(0, 0, 10, 100)}});

  // Initial max density is 10.
  EXPECT_EQ(tracker.MaxPageAdDensityByArea(), 10);

  // Go hidden.
  tracker.OnHidden();

  // Update ad rect to a larger size while hidden. Max density should not update
  // because calculations are skipped.
  tracker.UpdateMainFrameAdRects({{kRectId1, gfx::Rect(0, 0, 30, 100)}});
  EXPECT_EQ(tracker.MaxPageAdDensityByArea(), 10);

  // Go visible again. This should trigger recalculation. Max value is updated.
  tracker.OnShown();
  EXPECT_EQ(tracker.MaxPageAdDensityByArea(), 30);
}

TEST(PageAdDensityTrackerTest,
     StartInBackground_CalculationsDeferredUntilShown) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  // Construct the tracker for a page that starts in the background.
  PageAdDensityTracker tracker(/*is_in_foreground=*/false);

  // Update rects and check densities. They should be at their default values
  // because the page is backgrounded.
  tracker.UpdateMainFrameRect(gfx::Rect(0, 0, 100, 100));
  tracker.UpdateMainFrameViewportRect(gfx::Rect(0, 0, 100, 100));
  tracker.UpdateMainFrameAdRects({{kRectId1, gfx::Rect(0, 0, 50, 100)}});

  EXPECT_EQ(tracker.MaxPageAdDensityByArea(), -1);
  EXPECT_EQ(tracker.MaxPageAdDensityByHeight(), -1);

  // Stay hidden for 1s
  task_environment.FastForwardBy(base::Seconds(1));

  // Now, bring the page to the foreground.
  tracker.OnShown();

  // Check densities again. They should be calculated now.
  EXPECT_EQ(tracker.MaxPageAdDensityByArea(), 50);
  EXPECT_EQ(tracker.MaxPageAdDensityByHeight(), 100);

  // Stay visible for 1s
  task_environment.FastForwardBy(base::Seconds(1));
  tracker.Finalize();

  // Total visible time = 1s
  // 50% * 1s = 50
  // Average = 50 / 1.0 = 50.0
  EXPECT_DOUBLE_EQ(tracker.GetViewportAdDensityByAreaStats().mean, 50.0);
}

TEST(PageAdDensityTrackerTest, ViewportAdCount_SingleAd) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  PageAdDensityTracker tracker(/*is_in_foreground=*/true);

  tracker.UpdateMainFrameViewportRect(gfx::Rect(0, 0, 100, 100));
  tracker.UpdateMainFrameAdRects({{kRectId1, gfx::Rect(10, 10, 50, 50)}});

  task_environment.FastForwardBy(base::Seconds(1));
  tracker.Finalize();

  EXPECT_DOUBLE_EQ(tracker.GetViewportAdCountStats().mean, 1.0);
}

TEST(PageAdDensityTrackerTest, ViewportAdCount_MultipleAds) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  PageAdDensityTracker tracker(/*is_in_foreground=*/true);

  tracker.UpdateMainFrameViewportRect(gfx::Rect(0, 0, 100, 100));
  tracker.UpdateMainFrameAdRects({{kRectId1, gfx::Rect(10, 10, 20, 20)},
                                  {kRectId2, gfx::Rect(50, 50, 20, 20)}});

  task_environment.FastForwardBy(base::Seconds(1));
  tracker.Finalize();

  EXPECT_DOUBLE_EQ(tracker.GetViewportAdCountStats().mean, 2.0);
}

TEST(PageAdDensityTrackerTest, ViewportAdCount_AdOutOfViewport) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  PageAdDensityTracker tracker(/*is_in_foreground=*/true);

  tracker.UpdateMainFrameViewportRect(gfx::Rect(0, 0, 100, 100));
  tracker.UpdateMainFrameAdRects({{kRectId1, gfx::Rect(0, 100, 50, 50)}});

  task_environment.FastForwardBy(base::Seconds(1));
  tracker.Finalize();

  // Ad is outside the viewport, so the count should be 0.
  EXPECT_DOUBLE_EQ(tracker.GetViewportAdCountStats().mean, 0.0);
}

TEST(PageAdDensityTrackerTest, ViewportAdCount_PartialViewportOverlap) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  PageAdDensityTracker tracker(/*is_in_foreground=*/true);

  tracker.UpdateMainFrameViewportRect(gfx::Rect(0, 0, 100, 100));

  // Ad intersects the bottom right corner of the viewport.
  tracker.UpdateMainFrameAdRects({{kRectId1, gfx::Rect(80, 80, 50, 50)}});

  task_environment.FastForwardBy(base::Seconds(1));
  tracker.Finalize();

  // A partially overlapping ad is still counted as 1 whole ad.
  EXPECT_DOUBLE_EQ(tracker.GetViewportAdCountStats().mean, 1.0);
}

TEST(PageAdDensityTrackerTest, ViewportAdCount_OverlappingAds) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  PageAdDensityTracker tracker(/*is_in_foreground=*/true);

  tracker.UpdateMainFrameViewportRect(gfx::Rect(0, 0, 100, 100));
  tracker.UpdateMainFrameAdRects({
      {kRectId1, gfx::Rect(10, 10, 50, 50)},
      {kRectId2, gfx::Rect(20, 20, 50, 50)}  // Overlaps kRectId1
  });

  task_environment.FastForwardBy(base::Seconds(1));
  tracker.Finalize();

  // Unlike area density, the count should be 2 because there are two distinct
  // rects.
  EXPECT_DOUBLE_EQ(tracker.GetViewportAdCountStats().mean, 2.0);
}

TEST(PageAdDensityTrackerTest, ViewportAdCount_TimeWeightedAverage) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  PageAdDensityTracker tracker(/*is_in_foreground=*/true);

  tracker.UpdateMainFrameViewportRect(gfx::Rect(0, 0, 100, 100));
  tracker.UpdateMainFrameAdRects({{kRectId1, gfx::Rect(0, 0, 50, 50)}});

  task_environment.FastForwardBy(base::Seconds(1));

  // Scroll the viewport so the ad is no longer visible.
  tracker.UpdateMainFrameViewportRect(gfx::Rect(200, 200, 100, 100));

  task_environment.FastForwardBy(base::Seconds(1));

  tracker.Finalize();

  // 1 ad for 1 second, 0 ads for 1 second. Average = 0.5.
  EXPECT_DOUBLE_EQ(tracker.GetViewportAdCountStats().mean, 0.5);
}

}  // namespace page_load_metrics
