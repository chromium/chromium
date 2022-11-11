// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "base/test/task_environment.h"
#include "components/page_load_metrics/browser/observers/ad_metrics/page_ad_density_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace page_load_metrics {

// Only for test purpose.
class PageAdDensityTrackerTestPeer {
 public:
  static bool RectExistsAndHasCorrectTopIterator(
      const PageAdDensityTracker& tracker,
      int rect_id) {
    auto it = tracker.rect_events_iterators_.find(rect_id);
    // Rect not exists.
    if (it == tracker.rect_events_iterators_.end())
      return false;
    const PageAdDensityTracker::RectEventSetIterators& set_its = it->second;
    return !set_its.top_it->is_bottom;
  }

  static bool RectExistsAndHasCorrectBottomIterator(
      const PageAdDensityTracker& tracker,
      int rect_id) {
    auto it = tracker.rect_events_iterators_.find(rect_id);
    // Rect not exists.
    if (it == tracker.rect_events_iterators_.end())
      return false;
    const PageAdDensityTracker::RectEventSetIterators& set_its = it->second;
    return set_its.bottom_it->is_bottom;
  }
};

TEST(PageAdDensityTrackerTest, MultipleRects_MaxDensity) {
  PageAdDensityTracker tracker;

  // Page ad density is -1 before there is a main frame or subframes.
  EXPECT_EQ(tracker.MaxPageAdDensityByArea(), -1);

  tracker.UpdateMainFrameRect(gfx::Rect(0, 0, 100, 100));
  tracker.AddRect(1 /* rect_id */, gfx::Rect(0, 0, 100, 10));
  EXPECT_EQ(tracker.MaxPageAdDensityByArea(), 10);
  EXPECT_EQ(tracker.MaxPageAdDensityByHeight(), 10);

  tracker.AddRect(2 /* rect_id */, gfx::Rect(50, 0, 100, 20));
  EXPECT_EQ(tracker.MaxPageAdDensityByArea(), 15);
  EXPECT_EQ(tracker.MaxPageAdDensityByHeight(), 20);

  tracker.AddRect(3 /* rect_id */, gfx::Rect(50, 50, 50, 50));
  EXPECT_EQ(tracker.MaxPageAdDensityByArea(), 40);
  EXPECT_EQ(tracker.MaxPageAdDensityByHeight(), 70);

  // Removing a rect should not change the maximum ad density.
  tracker.RemoveRect(3 /* rect_id */);
  EXPECT_EQ(tracker.MaxPageAdDensityByArea(), 40);
  EXPECT_EQ(tracker.MaxPageAdDensityByHeight(), 70);
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
TEST(PageAdDensityTrackerTest, TwoRectsOverflowTotalAreaAndHeight) {
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

  // Density should be 0, as there's no intersected area.
  EXPECT_EQ(tracker.MaxPageAdDensityByArea(), 0);
  EXPECT_EQ(tracker.MaxPageAdDensityByHeight(), 0);
}

// Add a main frame rect whose area overflow an int.
TEST(PageAdDensityTrackerTest, OverflowTotalAreaAndHeight) {
  PageAdDensityTracker tracker;

  tracker.AddRect(1 /* rect_id */,
                  gfx::Rect(0, 0, std::numeric_limits<int>::max(),
                            std::numeric_limits<int>::max()));

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
  PageAdDensityTracker tracker;

  tracker.UpdateMainFrameRect(gfx::Rect(0, 0, 100, 100));

  tracker.AddRect(1 /* rect_id */, gfx::Rect(-1, -1, 1, 1));

  EXPECT_EQ(tracker.MaxPageAdDensityByArea(), 0);
  EXPECT_EQ(tracker.MaxPageAdDensityByHeight(), 0);
}

// Add a viewport rect whose area overflow an int.
TEST(PageAdDensityTrackerTest, ViewportAdDensity_OverflowViewportArea) {
  PageAdDensityTracker tracker;

  tracker.AddRect(1 /* rect_id */,
                  gfx::Rect(0, 0, std::numeric_limits<int>::max(),
                            std::numeric_limits<int>::max()));

  tracker.UpdateMainFrameViewportRect(gfx::Rect(
      0, 0, std::numeric_limits<int>::max(), std::numeric_limits<int>::max()));

  // Density should not be updated as the sum of ara overflows.
  EXPECT_EQ(tracker.ViewportAdDensityByArea(), 0);
}

TEST(PageAdDensityTrackerTest, ViewportAdDensity_RectSameSize) {
  PageAdDensityTracker tracker;

  tracker.UpdateMainFrameViewportRect(gfx::Rect(0, 0, 100, 100));
  tracker.AddRect(1 /* rect_id */, gfx::Rect(0, 0, 100, 100));

  EXPECT_EQ(tracker.ViewportAdDensityByArea(), 100);
}

TEST(PageAdDensityTrackerTest, ViewportAdDensity_RectHalfSize) {
  PageAdDensityTracker tracker;

  tracker.UpdateMainFrameViewportRect(gfx::Rect(0, 0, 100, 100));
  tracker.AddRect(1 /* rect_id */, gfx::Rect(0, 0, 50, 100));

  EXPECT_EQ(tracker.ViewportAdDensityByArea(), 50);
}

TEST(PageAdDensityTrackerTest, ViewportAdDensity_RectOutOfViewport) {
  PageAdDensityTracker tracker;

  tracker.UpdateMainFrameViewportRect(gfx::Rect(0, 0, 100, 100));
  tracker.AddRect(1 /* rect_id */, gfx::Rect(100, 0, 100, 100));

  EXPECT_EQ(tracker.ViewportAdDensityByArea(), 0);
}

TEST(PageAdDensityTrackerTest, ViewportAdDensity_RectClipsViewport) {
  PageAdDensityTracker tracker;

  tracker.UpdateMainFrameViewportRect(gfx::Rect(0, 0, 100, 100));
  tracker.AddRect(1 /* rect_id */, gfx::Rect(50, 50, 100, 100));

  EXPECT_EQ(tracker.ViewportAdDensityByArea(), 25);
}

TEST(PageAdDensityTrackerTest, ViewportAdDensity_TwoRectsClipViewport) {
  PageAdDensityTracker tracker;

  tracker.UpdateMainFrameViewportRect(gfx::Rect(0, 0, 100, 100));
  tracker.AddRect(1 /* rect_id */, gfx::Rect(30, 70, 100, 100));
  tracker.AddRect(2 /* rect_id */, gfx::Rect(70, 30, 100, 100));

  EXPECT_EQ(tracker.ViewportAdDensityByArea(),
            33);  // ((30 * 70 * 2) - 30 * 30) / 10000 * 100
}

TEST(PageAdDensityTrackerTest,
     AverageViewportAdDensity_NoTimeLapseSincePageLoad) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  PageAdDensityTracker tracker;

  tracker.UpdateMainFrameViewportRect(gfx::Rect(0, 0, 100, 100));
  tracker.AddRect(1 /* rect_id */, gfx::Rect(0, 0, 50, 50));

  tracker.Finalize();
  EXPECT_DOUBLE_EQ(tracker.GetAdDensityByAreaStats().mean, 0);
}

TEST(PageAdDensityTrackerTest, AverageViewportAdDensity_NoViewportRectUpdate) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  PageAdDensityTracker tracker;

  tracker.AddRect(1 /* rect_id */, gfx::Rect(0, 0, 50, 50));
  task_environment.FastForwardBy(base::Seconds(1));

  tracker.Finalize();
  EXPECT_DOUBLE_EQ(tracker.GetAdDensityByAreaStats().mean, 0);
}

TEST(PageAdDensityTrackerTest, AverageViewportAdDensity_NoAdRectUpdate) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  PageAdDensityTracker tracker;

  tracker.UpdateMainFrameViewportRect(gfx::Rect(0, 0, 100, 100));
  task_environment.FastForwardBy(base::Seconds(1));

  tracker.Finalize();
  EXPECT_DOUBLE_EQ(tracker.GetAdDensityByAreaStats().mean, 0);
}

TEST(PageAdDensityTrackerTest,
     AverageViewportAdDensity_CalculateInOneSecondAndImmediateQuery) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  PageAdDensityTracker tracker;

  task_environment.FastForwardBy(base::Seconds(1));

  tracker.UpdateMainFrameViewportRect(gfx::Rect(0, 0, 50, 50));
  tracker.AddRect(1 /* rect_id */, gfx::Rect(0, 0, 50, 50));

  EXPECT_EQ(tracker.ViewportAdDensityByArea(), 100);

  tracker.Finalize();
  EXPECT_DOUBLE_EQ(tracker.GetAdDensityByAreaStats().mean, 0);
}

TEST(PageAdDensityTrackerTest,
     AverageViewportAdDensity_CalculateInOneSecondAndQueryLater) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  PageAdDensityTracker tracker;

  task_environment.FastForwardBy(base::Seconds(1));

  tracker.UpdateMainFrameViewportRect(gfx::Rect(0, 0, 50, 50));
  tracker.AddRect(1 /* rect_id */, gfx::Rect(0, 0, 50, 50));

  task_environment.FastForwardBy(base::Seconds(1));

  EXPECT_EQ(tracker.ViewportAdDensityByArea(), 100);

  tracker.Finalize();
  EXPECT_DOUBLE_EQ(tracker.GetAdDensityByAreaStats().mean, 50);
}

TEST(PageAdDensityTrackerTest,
     AverageViewportAdDensity_ViewportRect_SizeUpdate) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  PageAdDensityTracker tracker;

  tracker.UpdateMainFrameViewportRect(gfx::Rect(0, 0, 50, 100));
  tracker.AddRect(1 /* rect_id */, gfx::Rect(0, 0, 50, 50));

  task_environment.FastForwardBy(base::Seconds(1));

  tracker.UpdateMainFrameViewportRect(gfx::Rect(0, 0, 50, 50));

  task_environment.FastForwardBy(base::Seconds(1));
  tracker.Finalize();
  EXPECT_DOUBLE_EQ(tracker.GetAdDensityByAreaStats().mean, 75);
}

TEST(PageAdDensityTrackerTest,
     AverageViewportAdDensity_ViewportRect_OffsetUpdate) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  PageAdDensityTracker tracker;

  tracker.AddRect(1 /* rect_id */, gfx::Rect(0, 0, 50, 50));
  tracker.UpdateMainFrameViewportRect(gfx::Rect(0, 0, 50, 50));

  task_environment.FastForwardBy(base::Seconds(1));

  tracker.UpdateMainFrameViewportRect(gfx::Rect(50, 50, 50, 50));

  task_environment.FastForwardBy(base::Seconds(1));
  tracker.Finalize();
  EXPECT_DOUBLE_EQ(tracker.GetAdDensityByAreaStats().mean, 50);
}

TEST(PageAdDensityTrackerTest, AverageViewportAdDensity_AdRectUpdate) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  PageAdDensityTracker tracker;

  tracker.AddRect(1 /* rect_id */, gfx::Rect(0, 0, 50, 50));
  tracker.UpdateMainFrameViewportRect(gfx::Rect(0, 0, 50, 100));

  task_environment.FastForwardBy(base::Seconds(1));

  tracker.RemoveRect(1 /* rect_id */);
  tracker.AddRect(1 /* rect_id */, gfx::Rect(0, 0, 50, 100));

  task_environment.FastForwardBy(base::Seconds(1));
  tracker.Finalize();
  EXPECT_DOUBLE_EQ(tracker.GetAdDensityByAreaStats().mean, 75);
}

TEST(PageAdDensityTrackerTest,
     AverageViewportAdDensity_MultipleUnequalTimePeriods) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  PageAdDensityTracker tracker;

  tracker.AddRect(1 /* rect_id */, gfx::Rect(0, 0, 50, 50));
  tracker.UpdateMainFrameViewportRect(gfx::Rect(0, 0, 50, 100));

  task_environment.FastForwardBy(base::Seconds(1));

  tracker.UpdateMainFrameViewportRect(gfx::Rect(25, 0, 50, 100));

  task_environment.FastForwardBy(base::Seconds(2));

  tracker.UpdateMainFrameViewportRect(gfx::Rect(50, 0, 50, 100));

  task_environment.FastForwardBy(base::Seconds(3));
  tracker.Finalize();
  EXPECT_DOUBLE_EQ(tracker.GetAdDensityByAreaStats().mean,
                   (50 * 1 + 25 * 2) / 6.0);
}

TEST(PageAdDensityTrackerTest,
     AverageViewportAdDensity_MultipleRectsInViewportRect) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  PageAdDensityTracker tracker;

  tracker.UpdateMainFrameViewportRect(gfx::Rect(50, 0, 50, 100));

  // Rect(1) is not within the viewport.
  tracker.AddRect(1 /* rect_id */, gfx::Rect(0, 0, 50, 50));

  // Rect(2) occupy 1/4 of the viewport.
  tracker.AddRect(2 /* rect_id */, gfx::Rect(25, 0, 50, 50));

  // Rect(3) occupy 1/4 of the viewport; 1/8 of the viewport is occupied by both
  // Rect(2) and Rect(3)
  tracker.AddRect(3 /* rect_id */, gfx::Rect(25, 25, 50, 50));

  task_environment.FastForwardBy(base::Seconds(1));
  tracker.Finalize();
  EXPECT_DOUBLE_EQ(tracker.GetAdDensityByAreaStats().mean, int(3 * 100 / 8));
}

TEST(PageAdDensityTrackerTest, RectEvent_CheckTopAndBottomIterator) {
  PageAdDensityTracker tracker;
  int rect_id = 1;
  tracker.AddRect(rect_id, gfx::Rect(0, 0, 50, 10));

  EXPECT_TRUE(PageAdDensityTrackerTestPeer::RectExistsAndHasCorrectTopIterator(
      tracker, rect_id));
  EXPECT_TRUE(
      PageAdDensityTrackerTestPeer::RectExistsAndHasCorrectBottomIterator(
          tracker, rect_id));
  tracker.RemoveRect(rect_id);
}

}  // namespace page_load_metrics
