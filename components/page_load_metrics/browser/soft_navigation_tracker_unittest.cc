// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/soft_navigation_tracker.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "components/page_load_metrics/browser/observers/core/largest_contentful_paint_handler.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace page_load_metrics {

namespace {

const content::GlobalRenderFrameHostToken kMainFrameToken{
    1, blink::LocalFrameToken()};

struct CompletedSoftNavigationRecord {
  uint64_t navigation_id = 0;
  InteractionToNextPaintCalculator inp_calculator;
  LayoutShiftNormalization cls_calculator;
  ContentfulPaintTimingInfo lcp;
};

class TestObserver : public SoftNavigationTracker::Client {
 public:
  void OnSoftNavigationFirstContentfulPaint(
      const mojom::SoftNavigationMetrics& metrics) override {
    fcps.push_back(metrics.Clone());
  }
  void OnSoftNavigationCompleted(const SoftNavigationData& data) override {
    if (data.metrics) {
      completed_nav_ids.push_back(
          data.metrics->performance_timeline_navigation_id);
      completed_navs.push_back({
          .navigation_id = data.metrics->performance_timeline_navigation_id,
          .inp_calculator = data.inp_calculator,
          .cls_calculator = data.cls_calculator,
          .lcp = data.lcp_handler.MergeMainFrameAndSubframes(),
      });
    }
  }

  std::vector<mojom::SoftNavigationMetricsPtr> fcps;
  std::vector<uint64_t> completed_nav_ids;
  std::vector<CompletedSoftNavigationRecord> completed_navs;
};

mojom::SoftNavigationMetricsPtr CreateSoftNavigationCommit(
    uint64_t nav_id,
    base::TimeDelta start_time,
    base::TimeTicks slicing_time,
    base::UnguessableToken token = base::UnguessableToken::Create(),
    blink::mojom::NavigationTypeForNavigationApi nav_type =
        blink::mojom::NavigationTypeForNavigationApi::kPush,
    std::optional<base::TimeDelta> fcp = std::nullopt) {
  auto entry = mojom::SoftNavigationMetrics::New();
  entry->performance_timeline_navigation_id = nav_id;
  entry->commit = mojom::SoftNavigationCommit::New(start_time, slicing_time,
                                                   nav_type, token);
  entry->first_contentful_paint = fcp;
  return entry;
}

mojom::SoftNavigationMetricsPtr CreateSoftNavigationFcpUpdate(
    uint64_t nav_id,
    base::TimeDelta first_contentful_paint) {
  auto entry = mojom::SoftNavigationMetrics::New();
  entry->performance_timeline_navigation_id = nav_id;
  entry->first_contentful_paint = first_contentful_paint;
  return entry;
}

}  // namespace

TEST(SoftNavigationTrackerTest, CountSoftNavigations) {
  base::TimeTicks base_time = base::TimeTicks::Now();
  TestObserver observer;
  SoftNavigationTracker tracker(&observer);
  std::vector<mojom::SoftNavigationMetricsPtr> soft_navigations;
  soft_navigations.push_back(CreateSoftNavigationCommit(
      2, base::Milliseconds(100), base_time + base::Milliseconds(150)));
  soft_navigations.push_back(CreateSoftNavigationCommit(
      3, base::Milliseconds(200), base_time + base::Milliseconds(250)));
  ASSERT_TRUE(tracker.UpdateMainFrameMetrics(kMainFrameToken,
                                             std::move(soft_navigations)));
  EXPECT_EQ(tracker.soft_navigation_count(), 2u);
}

TEST(SoftNavigationTrackerTest, FirstContentfulPaintDispatchesObserverEvent) {
  base::TimeTicks base_time = base::TimeTicks::Now();
  TestObserver observer;
  SoftNavigationTracker tracker(&observer);

  // 1. Commit arrives without FCP.
  std::vector<mojom::SoftNavigationMetricsPtr> nav_commits;
  nav_commits.push_back(CreateSoftNavigationCommit(
      2, base::Milliseconds(100), base_time + base::Milliseconds(150)));
  ASSERT_TRUE(
      tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(nav_commits)));
  EXPECT_EQ(tracker.soft_navigation_count(), 1u);
  EXPECT_EQ(observer.fcps.size(), 0u);

  // Standalone FCP arrives for navigation 2.
  std::vector<mojom::SoftNavigationMetricsPtr> nav_fcps;
  nav_fcps.push_back(CreateSoftNavigationFcpUpdate(2, base::Milliseconds(120)));
  ASSERT_TRUE(
      tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(nav_fcps)));
  EXPECT_EQ(observer.fcps.size(), 1u);
  EXPECT_EQ(tracker.soft_navigation_count(), 1u);
  EXPECT_EQ(observer.fcps[0]->performance_timeline_navigation_id, 2u);
  EXPECT_EQ(observer.fcps[0]->first_contentful_paint, base::Milliseconds(120));

  // Duplicate FCP is rejected and does not dispatch another event.
  std::vector<mojom::SoftNavigationMetricsPtr> duplicate_fcps;
  duplicate_fcps.push_back(
      CreateSoftNavigationFcpUpdate(2, base::Milliseconds(120)));
  ASSERT_FALSE(tracker.UpdateMainFrameMetrics(kMainFrameToken,
                                              std::move(duplicate_fcps)));
  EXPECT_EQ(observer.fcps.size(), 1u);
  EXPECT_EQ(tracker.soft_navigation_count(), 1u);

  // 2. Navigation 3 arrives with bundled Commit and FCP.
  std::vector<mojom::SoftNavigationMetricsPtr> bundled_navs;
  bundled_navs.push_back(CreateSoftNavigationCommit(
      3, base::Milliseconds(200), base_time + base::Milliseconds(250),
      base::UnguessableToken::Create(),
      blink::mojom::NavigationTypeForNavigationApi::kPush,
      base::Milliseconds(230)));
  ASSERT_TRUE(
      tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(bundled_navs)));
  EXPECT_EQ(tracker.soft_navigation_count(), 2u);
  EXPECT_EQ(observer.fcps.size(), 2u);
  EXPECT_EQ(observer.fcps[1]->performance_timeline_navigation_id, 3u);
  EXPECT_EQ(observer.fcps[1]->first_contentful_paint, base::Milliseconds(230));
  ASSERT_EQ(observer.completed_nav_ids.size(), 1u);
  EXPECT_EQ(observer.completed_nav_ids[0], 2u);
}

TEST(SoftNavigationTrackerTest,
     OutOfOrderFirstContentfulPaintDispatchedInFifoOrder) {
  base::TimeTicks base_time = base::TimeTicks::Now();
  TestObserver observer;
  SoftNavigationTracker tracker(&observer);

  // 1. Navigation 2 and Navigation 3 commit in order without FCP.
  std::vector<mojom::SoftNavigationMetricsPtr> nav_commits;
  nav_commits.push_back(CreateSoftNavigationCommit(
      2, base::Milliseconds(100), base_time + base::Milliseconds(150)));
  nav_commits.push_back(CreateSoftNavigationCommit(
      3, base::Milliseconds(200), base_time + base::Milliseconds(250)));
  ASSERT_TRUE(
      tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(nav_commits)));
  EXPECT_EQ(tracker.soft_navigation_count(), 2u);
  EXPECT_TRUE(observer.fcps.empty());
  EXPECT_TRUE(observer.completed_nav_ids.empty());

  // 2. Standalone FCP arrives for Navigation 3 FIRST (out of order).
  // Neither FCP nor completion should be dispatched yet because Navigation 2
  // has not presented FCP.
  std::vector<mojom::SoftNavigationMetricsPtr> nav3_fcp;
  nav3_fcp.push_back(CreateSoftNavigationFcpUpdate(3, base::Milliseconds(220)));
  ASSERT_TRUE(
      tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(nav3_fcp)));
  EXPECT_TRUE(observer.fcps.empty());
  EXPECT_TRUE(observer.completed_nav_ids.empty());

  // 3. Standalone FCP arrives for Navigation 2.
  // Events must be dispatched in FIFO order:
  // Navigation 2 FCP -> Navigation 2 Completed -> Navigation 3 FCP.
  std::vector<mojom::SoftNavigationMetricsPtr> nav2_fcp;
  nav2_fcp.push_back(CreateSoftNavigationFcpUpdate(2, base::Milliseconds(120)));
  ASSERT_TRUE(
      tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(nav2_fcp)));

  EXPECT_EQ(observer.fcps.size(), 2u);
  EXPECT_EQ(observer.fcps[0]->performance_timeline_navigation_id, 2u);
  EXPECT_EQ(observer.fcps[0]->first_contentful_paint, base::Milliseconds(120));
  EXPECT_EQ(observer.fcps[1]->performance_timeline_navigation_id, 3u);
  EXPECT_EQ(observer.fcps[1]->first_contentful_paint, base::Milliseconds(220));
  ASSERT_EQ(observer.completed_nav_ids.size(), 1u);
  EXPECT_EQ(observer.completed_nav_ids[0], 2u);
}

TEST(SoftNavigationTrackerTest,
     UpdateMainFrameMetricsValidatesIncomingMetrics) {
  base::TimeTicks base_time = base::TimeTicks::Now();
  TestObserver observer;

  {
    // Empty metric is rejected.
    SoftNavigationTracker tracker(&observer);
    std::vector<mojom::SoftNavigationMetricsPtr> soft_navigations;
    soft_navigations.push_back(mojom::SoftNavigationMetrics::New());
    EXPECT_FALSE(tracker.UpdateMainFrameMetrics(kMainFrameToken,
                                                std::move(soft_navigations)));
  }

  {
    // Slicing time missing.
    SoftNavigationTracker tracker(&observer);
    std::vector<mojom::SoftNavigationMetricsPtr> soft_navigations;
    auto entry = mojom::SoftNavigationMetrics::New();
    entry->performance_timeline_navigation_id = 2;
    entry->commit = mojom::SoftNavigationCommit::New();
    entry->commit->start_time = base::Milliseconds(100);
    entry->commit->same_document_metrics_token =
        base::UnguessableToken::Create();
    soft_navigations.push_back(std::move(entry));
    EXPECT_FALSE(tracker.UpdateMainFrameMetrics(kMainFrameToken,
                                                std::move(soft_navigations)));
  }

  {
    // same_document_metrics_token missing.
    SoftNavigationTracker tracker(&observer);
    std::vector<mojom::SoftNavigationMetricsPtr> soft_navigations;
    auto entry = mojom::SoftNavigationMetrics::New();
    entry->performance_timeline_navigation_id = 2;
    entry->commit = mojom::SoftNavigationCommit::New();
    entry->commit->start_time = base::Milliseconds(100);
    entry->commit->soft_navigation_slicing_time =
        base_time + base::Milliseconds(150);
    soft_navigations.push_back(std::move(entry));
    EXPECT_FALSE(tracker.UpdateMainFrameMetrics(kMainFrameToken,
                                                std::move(soft_navigations)));
  }

  {
    // Metric with navigation_id < 2 is rejected.
    SoftNavigationTracker tracker(&observer);
    std::vector<mojom::SoftNavigationMetricsPtr> soft_navigations;
    soft_navigations.push_back(CreateSoftNavigationCommit(
        1, base::Milliseconds(100), base_time + base::Milliseconds(150)));
    EXPECT_FALSE(tracker.UpdateMainFrameMetrics(kMainFrameToken,
                                                std::move(soft_navigations)));
  }

  {
    // Metric starting with navigation_id > 2 is accepted.
    SoftNavigationTracker tracker(&observer);
    std::vector<mojom::SoftNavigationMetricsPtr> soft_navigations;
    soft_navigations.push_back(CreateSoftNavigationCommit(
        3, base::Milliseconds(100), base_time + base::Milliseconds(150)));
    EXPECT_TRUE(tracker.UpdateMainFrameMetrics(kMainFrameToken,
                                               std::move(soft_navigations)));
  }

  {
    // Slicing time is not monotonically increasing.
    SoftNavigationTracker tracker(&observer);
    std::vector<mojom::SoftNavigationMetricsPtr> soft_navigations;
    soft_navigations.push_back(CreateSoftNavigationCommit(
        2, base::Milliseconds(100), base_time + base::Milliseconds(150)));
    soft_navigations.push_back(CreateSoftNavigationCommit(
        3, base::Milliseconds(200), base_time + base::Milliseconds(140)));
    EXPECT_FALSE(tracker.UpdateMainFrameMetrics(kMainFrameToken,
                                                std::move(soft_navigations)));
  }

  {
    // The same_document_metrics_token is the same for subsequent soft
    // navigations.
    SoftNavigationTracker tracker(&observer);
    std::vector<mojom::SoftNavigationMetricsPtr> soft_navigations;
    base::UnguessableToken token = base::UnguessableToken::Create();
    soft_navigations.push_back(
        CreateSoftNavigationCommit(2, base::Milliseconds(100),
                                   base_time + base::Milliseconds(150), token));
    EXPECT_TRUE(tracker.UpdateMainFrameMetrics(kMainFrameToken,
                                               std::move(soft_navigations)));

    std::vector<mojom::SoftNavigationMetricsPtr> soft_navigations2;
    soft_navigations2.push_back(
        CreateSoftNavigationCommit(3, base::Milliseconds(200),
                                   base_time + base::Milliseconds(250), token));
    EXPECT_FALSE(tracker.UpdateMainFrameMetrics(kMainFrameToken,
                                                std::move(soft_navigations2)));
  }

  {
    // Standalone metric update without metric data (e.g. missing FCP) is
    // rejected.
    SoftNavigationTracker tracker(&observer);
    std::vector<mojom::SoftNavigationMetricsPtr> soft_navigations;
    auto entry = mojom::SoftNavigationMetrics::New();
    entry->performance_timeline_navigation_id = 2;
    soft_navigations.push_back(std::move(entry));
    EXPECT_FALSE(tracker.UpdateMainFrameMetrics(kMainFrameToken,
                                                std::move(soft_navigations)));
  }

  {
    // Standalone FCP updates can arrive out of order for different navigations.
    SoftNavigationTracker tracker(&observer);
    std::vector<mojom::SoftNavigationMetricsPtr> soft_navigations;
    soft_navigations.push_back(
        CreateSoftNavigationFcpUpdate(3, base::Milliseconds(200)));
    soft_navigations.push_back(
        CreateSoftNavigationFcpUpdate(2, base::Milliseconds(100)));
    EXPECT_TRUE(tracker.UpdateMainFrameMetrics(kMainFrameToken,
                                               std::move(soft_navigations)));
  }

  {
    // Multiple FCP updates for the same navigation in a single batch are
    // rejected.
    SoftNavigationTracker tracker(&observer);
    std::vector<mojom::SoftNavigationMetricsPtr> soft_navigations;
    soft_navigations.push_back(
        CreateSoftNavigationFcpUpdate(2, base::Milliseconds(100)));
    soft_navigations.push_back(
        CreateSoftNavigationFcpUpdate(2, base::Milliseconds(120)));
    EXPECT_FALSE(tracker.UpdateMainFrameMetrics(kMainFrameToken,
                                                std::move(soft_navigations)));
  }
}

TEST(SoftNavigationTrackerTest, TrackerRecoversAfterInvalidMetrics) {
  base::TimeTicks base_time = base::TimeTicks::Now();
  TestObserver observer;
  mojom::SoftNavigationMetricsPtr valid_soft_navigation =
      CreateSoftNavigationCommit(2, base::Milliseconds(100),
                                 base_time + base::Milliseconds(150));
  {
    // Verify that this is a valid soft navigation.
    SoftNavigationTracker tracker(&observer);
    std::vector<mojom::SoftNavigationMetricsPtr> soft_navigations;
    soft_navigations.push_back(valid_soft_navigation->Clone());
    EXPECT_TRUE(tracker.UpdateMainFrameMetrics(kMainFrameToken,
                                               std::move(soft_navigations)));
  }

  SoftNavigationTracker tracker(&observer);
  std::vector<mojom::SoftNavigationMetricsPtr> soft_navigations;
  // This first metric is valid, but the second one is not (it's empty).
  // Therefore, the tracker should not accept any metrics, and
  // UpdateMainFrameMetrics should return false.
  soft_navigations.push_back(valid_soft_navigation->Clone());
  soft_navigations.push_back(mojom::SoftNavigationMetrics::New());
  EXPECT_FALSE(tracker.UpdateMainFrameMetrics(kMainFrameToken,
                                              std::move(soft_navigations)));
  // But, after this, the tracker should be in a good state, so we can test
  // that it accepts metrics in the next call.
  soft_navigations.clear();
  soft_navigations.push_back(valid_soft_navigation->Clone());
  soft_navigations.push_back(CreateSoftNavigationCommit(
      3, base::Milliseconds(200), base_time + base::Milliseconds(250)));
  EXPECT_TRUE(tracker.UpdateMainFrameMetrics(kMainFrameToken,
                                             std::move(soft_navigations)));
  EXPECT_EQ(tracker.soft_navigation_count(), 2u);
}

TEST(SoftNavigationTrackerTest,
     SimpleSoftNavsTrackingAndAggregationForInteractions) {
  base::TimeTicks base_time = base::TimeTicks::Now();
  std::vector<mojom::SoftNavigationMetricsPtr> soft_navigations;
  soft_navigations.push_back(CreateSoftNavigationCommit(
      2, base::Milliseconds(80), base_time + base::Milliseconds(100),
      /*token=*/base::UnguessableToken::Create(),
      /*nav_type=*/blink::mojom::NavigationTypeForNavigationApi::kPush,
      /*fcp=*/base::Milliseconds(90)));
  soft_navigations.push_back(CreateSoftNavigationCommit(
      3, base::Milliseconds(180), base_time + base::Milliseconds(200),
      /*token=*/base::UnguessableToken::Create(),
      /*nav_type=*/blink::mojom::NavigationTypeForNavigationApi::kPush,
      /*fcp=*/base::Milliseconds(190)));
  std::vector<mojom::EventTimingPtr> latencies;
  // Before the first soft navigation (hard nav, nav_id = 1), there are two
  // user interactions.
  latencies.push_back(mojom::EventTiming::New());
  latencies.back()->processing_start = base_time + base::Milliseconds(50);
  latencies.back()->duration = base::Milliseconds(10);
  latencies.back()->interaction_id = 1;
  latencies.back()->performance_timeline_navigation_id = 1;
  latencies.push_back(mojom::EventTiming::New());
  latencies.back()->processing_start = base_time + base::Milliseconds(80);
  latencies.back()->duration = base::Milliseconds(20);
  latencies.back()->interaction_id = 2;
  latencies.back()->performance_timeline_navigation_id = 1;
  // After the first soft navigation (nav_id = 2), there is one user
  // interaction.
  latencies.push_back(mojom::EventTiming::New());
  latencies.back()->processing_start = base_time + base::Milliseconds(150);
  latencies.back()->duration = base::Milliseconds(30);
  latencies.back()->interaction_id = 3;
  latencies.back()->performance_timeline_navigation_id = 2;
  // After the second soft navigation (nav_id = 3), there is another user
  // interaction.
  latencies.push_back(mojom::EventTiming::New());
  latencies.back()->processing_start = base_time + base::Milliseconds(250);
  latencies.back()->duration = base::Milliseconds(20);
  latencies.back()->interaction_id = 4;
  latencies.back()->performance_timeline_navigation_id = 3;

  TestObserver observer;
  SoftNavigationTracker tracker(&observer);
  ASSERT_TRUE(tracker.UpdateMainFrameMetrics(
      kMainFrameToken, std::move(soft_navigations), latencies));
  tracker.CompleteActiveNavigationAndFlush();

  EXPECT_EQ(tracker.soft_navigation_count(), 2u);
  ASSERT_EQ(observer.completed_navs.size(), 2u);

  // Check first soft navigation (nav_id = 2).
  const auto& nav2 = observer.completed_navs[0];
  EXPECT_EQ(nav2.navigation_id, 2u);
  EXPECT_EQ(nav2.inp_calculator.num_user_interactions(), 1u);
  ASSERT_TRUE(nav2.inp_calculator.worst_latency().has_value());
  EXPECT_EQ(nav2.inp_calculator.worst_latency()->max_event.duration,
            base::Milliseconds(30));
  ASSERT_TRUE(nav2.inp_calculator.ApproximateHighPercentile().has_value());
  EXPECT_EQ(nav2.inp_calculator.ApproximateHighPercentile()->max_event.duration,
            base::Milliseconds(30));

  // Check second soft navigation (nav_id = 3).
  const auto& nav3 = observer.completed_navs[1];
  EXPECT_EQ(nav3.navigation_id, 3u);
  EXPECT_EQ(nav3.inp_calculator.num_user_interactions(), 1u);
  ASSERT_TRUE(nav3.inp_calculator.worst_latency().has_value());
  EXPECT_EQ(nav3.inp_calculator.worst_latency()->max_event.duration,
            base::Milliseconds(20));
  ASSERT_TRUE(nav3.inp_calculator.ApproximateHighPercentile().has_value());
  EXPECT_EQ(nav3.inp_calculator.ApproximateHighPercentile()->max_event.duration,
            base::Milliseconds(20));
}

TEST(SoftNavigationTrackerTest,
     SimpleSoftNavsTrackingAndAggregationForLayoutShifts) {
  base::TimeTicks base_time = base::TimeTicks::Now();
  std::vector<mojom::SoftNavigationMetricsPtr> soft_navigations;
  soft_navigations.push_back(CreateSoftNavigationCommit(
      2, base::Milliseconds(80), base_time + base::Milliseconds(100),
      /*token=*/base::UnguessableToken::Create(),
      /*nav_type=*/blink::mojom::NavigationTypeForNavigationApi::kPush,
      /*fcp=*/base::Milliseconds(90)));
  soft_navigations.push_back(CreateSoftNavigationCommit(
      3, base::Milliseconds(180), base_time + base::Milliseconds(200),
      /*token=*/base::UnguessableToken::Create(),
      /*nav_type=*/blink::mojom::NavigationTypeForNavigationApi::kPush,
      /*fcp=*/base::Milliseconds(190)));

  std::vector<mojom::LayoutShiftPtr> layout_shifts;
  // Before the first soft navigation (hard nav, nav_id = 1), there are two
  // layout shifts.
  layout_shifts.push_back(mojom::LayoutShift::New());
  layout_shifts.back()->layout_shift_time = base_time + base::Milliseconds(50);
  layout_shifts.back()->layout_shift_score = 0.1;
  layout_shifts.back()->performance_timeline_navigation_id = 1;
  layout_shifts.push_back(mojom::LayoutShift::New());
  layout_shifts.back()->layout_shift_time = base_time + base::Milliseconds(80);
  layout_shifts.back()->layout_shift_score = 0.21;
  layout_shifts.back()->performance_timeline_navigation_id = 1;
  // After the first soft navigation (nav_id = 2), there is one layout shift.
  layout_shifts.push_back(mojom::LayoutShift::New());
  layout_shifts.back()->layout_shift_time = base_time + base::Milliseconds(150);
  layout_shifts.back()->layout_shift_score = 0.3;
  layout_shifts.back()->performance_timeline_navigation_id = 2;
  // After the second soft navigation (nav_id = 3), there is another layout
  // shift.
  layout_shifts.push_back(mojom::LayoutShift::New());
  layout_shifts.back()->layout_shift_time = base_time + base::Milliseconds(250);
  layout_shifts.back()->layout_shift_score = 0.4;
  layout_shifts.back()->performance_timeline_navigation_id = 3;

  TestObserver observer;
  SoftNavigationTracker tracker(&observer);
  ASSERT_TRUE(tracker.UpdateMainFrameMetrics(
      kMainFrameToken, std::move(soft_navigations), {}, layout_shifts));
  tracker.CompleteActiveNavigationAndFlush();

  EXPECT_EQ(tracker.soft_navigation_count(), 2u);
  ASSERT_EQ(observer.completed_navs.size(), 2u);

  const auto& nav2 = observer.completed_navs[0];
  EXPECT_EQ(nav2.navigation_id, 2u);
  EXPECT_FLOAT_EQ(nav2.cls_calculator.normalized_cls_data()
                      .session_windows_gap1000ms_max5000ms_max_cls,
                  0.3f);

  const auto& nav3 = observer.completed_navs[1];
  EXPECT_EQ(nav3.navigation_id, 3u);
  EXPECT_FLOAT_EQ(nav3.cls_calculator.normalized_cls_data()
                      .session_windows_gap1000ms_max5000ms_max_cls,
                  0.4f);
}

TEST(SoftNavigationTrackerTest,
     SimpleSoftNavsTrackingAndAggregationForLargestContentfulPaint) {
  base::TimeTicks base_time = base::TimeTicks::Now();

  std::vector<mojom::SoftNavigationMetricsPtr> soft_navigations;
  soft_navigations.push_back(CreateSoftNavigationCommit(
      2, base::Milliseconds(80), base_time + base::Milliseconds(100),
      /*token=*/base::UnguessableToken::Create(),
      /*nav_type=*/blink::mojom::NavigationTypeForNavigationApi::kPush,
      /*fcp=*/base::Milliseconds(90)));
  soft_navigations.push_back(CreateSoftNavigationCommit(
      3, base::Milliseconds(180), base_time + base::Milliseconds(200),
      /*token=*/base::UnguessableToken::Create(),
      /*nav_type=*/blink::mojom::NavigationTypeForNavigationApi::kPush,
      /*fcp=*/base::Milliseconds(190)));

  std::vector<mojom::LargestContentfulPaintTimingPtr> lcps;

  // Before the first soft navigation (hard nav, nav_id = 1), there is one
  // LCP candidate.
  lcps.push_back(mojom::LargestContentfulPaintTiming::New());
  lcps.back()->largest_text_paint = base::Milliseconds(50);
  lcps.back()->largest_text_paint_size = 400;
  lcps.back()->type = 0;
  lcps.back()->performance_timeline_navigation_id = 1;
  lcps.back()->resource_load_timings = mojom::LcpResourceLoadTimings::New();

  // During the first soft navigation (nav_id = 2), there are two LCP candidates
  // (text + image).
  lcps.push_back(mojom::LargestContentfulPaintTiming::New());
  lcps.back()->largest_text_paint = base::Milliseconds(120);
  lcps.back()->largest_text_paint_size = 50;
  lcps.back()->type = 0;
  lcps.back()->performance_timeline_navigation_id = 2;
  lcps.back()->resource_load_timings = mojom::LcpResourceLoadTimings::New();

  lcps.push_back(mojom::LargestContentfulPaintTiming::New());
  lcps.back()->largest_image_paint = base::Milliseconds(150);
  lcps.back()->largest_image_paint_size = 375;
  lcps.back()->type = 0;
  lcps.back()->performance_timeline_navigation_id = 2;
  lcps.back()->resource_load_timings = mojom::LcpResourceLoadTimings::New();

  // During the second soft navigation (nav_id = 3), there is one LCP candidate
  // (image).
  lcps.push_back(mojom::LargestContentfulPaintTiming::New());
  lcps.back()->largest_image_paint = base::Milliseconds(240);
  lcps.back()->largest_image_paint_size = 800;
  lcps.back()->type = 0;
  lcps.back()->performance_timeline_navigation_id = 3;
  lcps.back()->resource_load_timings = mojom::LcpResourceLoadTimings::New();

  TestObserver observer;
  SoftNavigationTracker tracker(&observer);
  ASSERT_TRUE(tracker.UpdateMainFrameMetrics(
      kMainFrameToken, std::move(soft_navigations), {}, {}, lcps));
  tracker.CompleteActiveNavigationAndFlush();

  EXPECT_EQ(tracker.soft_navigation_count(), 2u);
  ASSERT_EQ(observer.completed_navs.size(), 2u);

  const auto& nav2 = observer.completed_navs[0];
  EXPECT_EQ(nav2.navigation_id, 2u);
  EXPECT_EQ(nav2.lcp.Size(), 375u);
  EXPECT_EQ(nav2.lcp.Time().value(), base::Milliseconds(150));

  const auto& nav3 = observer.completed_navs[1];
  EXPECT_EQ(nav3.navigation_id, 3u);
  EXPECT_EQ(nav3.lcp.Size(), 800u);
  EXPECT_EQ(nav3.lcp.Time().value(), base::Milliseconds(240));
}

TEST(SoftNavigationTrackerTest, IncrementalSoftNavigationUpdates) {
  base::TimeTicks base_time = base::TimeTicks::Now();
  TestObserver observer;
  SoftNavigationTracker tracker(&observer);

  // Step 1: Soft Nav 1 arrives (nav_id = 2).
  std::vector<mojom::SoftNavigationMetricsPtr> soft_navs_1;
  soft_navs_1.push_back(CreateSoftNavigationCommit(
      2, base::Milliseconds(80), base_time + base::Milliseconds(100)));
  EXPECT_TRUE(
      tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(soft_navs_1)));
  EXPECT_EQ(tracker.soft_navigation_count(), 1u);
  EXPECT_NE(tracker.GetSoftNavigationDataForTest(2), nullptr);
  EXPECT_TRUE(observer.completed_nav_ids.empty());

  // Step 2: Events for Soft Nav 1 arrive.
  std::vector<mojom::EventTimingPtr> events_1;
  events_1.push_back(mojom::EventTiming::New());
  events_1.back()->processing_start = base_time + base::Milliseconds(150);
  events_1.back()->duration = base::Milliseconds(20);
  events_1.back()->interaction_id = 2;
  events_1.back()->performance_timeline_navigation_id = 2;
  EXPECT_TRUE(tracker.UpdateMainFrameMetrics(
      kMainFrameToken, /*soft_navigation_metrics=*/{}, events_1));

  const auto* nav2 = tracker.GetSoftNavigationDataForTest(2);
  ASSERT_TRUE(nav2);
  EXPECT_EQ(nav2->inp_calculator.num_user_interactions(), 1u);
  EXPECT_EQ(nav2->inp_calculator.worst_latency()->max_event.duration,
            base::Milliseconds(20));

  // Step 3: Soft Nav 2 arrives (nav_id = 3).
  std::vector<mojom::SoftNavigationMetricsPtr> soft_navs_2;
  soft_navs_2.push_back(CreateSoftNavigationCommit(
      3, base::Milliseconds(180), base_time + base::Milliseconds(200)));
  EXPECT_TRUE(
      tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(soft_navs_2)));

  // Soft Nav 1 is in `completed_navigations_awaiting_reporting_criteria_`.
  // It has not been dispatched yet because it is awaiting its own FCP and the
  // next navigation's FCP.
  EXPECT_TRUE(observer.completed_nav_ids.empty());
  EXPECT_EQ(tracker.soft_navigation_count(), 2u);
  EXPECT_NE(tracker.GetSoftNavigationDataForTest(3), nullptr);

  // Soft Nav 1 is still accessible while in the queue, so in-flight events for
  // Soft Nav 1 continue to be aggregated.
  std::vector<mojom::EventTimingPtr> late_events;
  late_events.push_back(mojom::EventTiming::New());
  late_events.back()->processing_start = base_time + base::Milliseconds(180);
  late_events.back()->duration = base::Milliseconds(30);
  late_events.back()->interaction_id = 3;
  late_events.back()->performance_timeline_navigation_id = 2;
  EXPECT_TRUE(tracker.UpdateMainFrameMetrics(
      kMainFrameToken, /*soft_navigation_metrics=*/{}, late_events));

  const auto* nav2_in_queue = tracker.GetSoftNavigationDataForTest(2);
  ASSERT_TRUE(nav2_in_queue);
  EXPECT_EQ(nav2_in_queue->inp_calculator.num_user_interactions(), 2u);

  // Step 4: FCP arrives for Soft Nav 1.
  std::vector<mojom::SoftNavigationMetricsPtr> fcp_nav1;
  fcp_nav1.push_back(CreateSoftNavigationFcpUpdate(2, base::Milliseconds(90)));
  EXPECT_TRUE(
      tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(fcp_nav1)));
  // Still awaiting Soft Nav 2's FCP proxy before dispatching.
  EXPECT_TRUE(observer.completed_nav_ids.empty());

  // Step 5: Events for active Soft Nav 2 (nav_id = 3) are recorded.
  std::vector<mojom::EventTimingPtr> events_2;
  events_2.push_back(mojom::EventTiming::New());
  events_2.back()->processing_start = base_time + base::Milliseconds(250);
  events_2.back()->duration = base::Milliseconds(40);
  events_2.back()->interaction_id = 4;
  events_2.back()->performance_timeline_navigation_id = 3;
  EXPECT_TRUE(tracker.UpdateMainFrameMetrics(
      kMainFrameToken, /*soft_navigation_metrics=*/{}, events_2));

  const auto* nav3 = tracker.GetSoftNavigationDataForTest(3);
  ASSERT_TRUE(nav3);
  EXPECT_EQ(nav3->inp_calculator.num_user_interactions(), 1u);
  EXPECT_EQ(nav3->inp_calculator.worst_latency()->max_event.duration,
            base::Milliseconds(40));

  // Step 6: FCP arrives for Soft Nav 2. Soft Nav 1 now satisfies all reporting
  // criteria and is dispatched!
  std::vector<mojom::SoftNavigationMetricsPtr> fcp_nav2;
  fcp_nav2.push_back(CreateSoftNavigationFcpUpdate(3, base::Milliseconds(190)));
  EXPECT_TRUE(
      tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(fcp_nav2)));

  ASSERT_EQ(observer.completed_nav_ids.size(), 1u);
  EXPECT_EQ(observer.completed_nav_ids[0], 2u);
  EXPECT_EQ(tracker.GetSoftNavigationDataForTest(2), nullptr);

  // Late-arriving events for the already dispatched Soft Nav 1 are ignored.
  std::vector<mojom::EventTimingPtr> ignored_events;
  ignored_events.push_back(mojom::EventTiming::New());
  ignored_events.back()->processing_start = base_time + base::Milliseconds(260);
  ignored_events.back()->duration = base::Milliseconds(50);
  ignored_events.back()->interaction_id = 5;
  ignored_events.back()->performance_timeline_navigation_id = 2;
  EXPECT_TRUE(tracker.UpdateMainFrameMetrics(
      kMainFrameToken, /*soft_navigation_metrics=*/{}, ignored_events));
  EXPECT_EQ(tracker.GetSoftNavigationDataForTest(2), nullptr);

  // Finalize all active navigations and take completed navigations on flush.
  tracker.CompleteActiveNavigationAndFlush();
  ASSERT_EQ(observer.completed_nav_ids.size(), 2u);
  EXPECT_EQ(observer.completed_nav_ids[1], 3u);
  EXPECT_EQ(tracker.GetSoftNavigationDataForTest(3), nullptr);
}

TEST(SoftNavigationTrackerTest, CompletedNavigationsAwaitingReportingCriteria) {
  base::TimeTicks base_time = base::TimeTicks::Now();
  TestObserver observer;
  SoftNavigationTracker tracker(&observer);

  // Nav 1 (id=2), Nav 2 (id=3), Nav 3 (id=4) commit in succession without FCPs.
  std::vector<mojom::SoftNavigationMetricsPtr> navs;
  navs.push_back(CreateSoftNavigationCommit(
      2, base::Milliseconds(100), base_time + base::Milliseconds(100)));
  navs.push_back(CreateSoftNavigationCommit(
      3, base::Milliseconds(200), base_time + base::Milliseconds(200)));
  navs.push_back(CreateSoftNavigationCommit(
      4, base::Milliseconds(300), base_time + base::Milliseconds(300)));
  EXPECT_TRUE(tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(navs)));

  EXPECT_EQ(tracker.soft_navigation_count(), 3u);
  EXPECT_TRUE(observer.completed_nav_ids.empty());

  // Nav 1 gets FCP -> still in queue because Nav 2 has no FCP.
  std::vector<mojom::SoftNavigationMetricsPtr> fcp_1;
  fcp_1.push_back(CreateSoftNavigationFcpUpdate(2, base::Milliseconds(110)));
  EXPECT_TRUE(
      tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(fcp_1)));
  EXPECT_TRUE(observer.completed_nav_ids.empty());

  // Nav 2 gets FCP -> Nav 1 has all criteria (own FCP + next nav FCP) and
  // completes! Nav 2 remains in queue awaiting Nav 3's FCP.
  std::vector<mojom::SoftNavigationMetricsPtr> fcp_2;
  fcp_2.push_back(CreateSoftNavigationFcpUpdate(3, base::Milliseconds(210)));
  EXPECT_TRUE(
      tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(fcp_2)));
  ASSERT_EQ(observer.completed_nav_ids.size(), 1u);
  EXPECT_EQ(observer.completed_nav_ids[0], 2u);

  // Nav 3 gets FCP -> Nav 2 completes!
  std::vector<mojom::SoftNavigationMetricsPtr> fcp_3;
  fcp_3.push_back(CreateSoftNavigationFcpUpdate(4, base::Milliseconds(310)));
  EXPECT_TRUE(
      tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(fcp_3)));
  ASSERT_EQ(observer.completed_nav_ids.size(), 2u);
  EXPECT_EQ(observer.completed_nav_ids[1], 3u);

  // Flush completes active Nav 3.
  tracker.CompleteActiveNavigationAndFlush();
  ASSERT_EQ(observer.completed_nav_ids.size(), 3u);
  EXPECT_EQ(observer.completed_nav_ids[2], 4u);
}

TEST(SoftNavigationTrackerTest, MaxSoftNavigationsCap) {
  TestObserver observer;
  SoftNavigationTracker tracker(&observer);

  // Send uncommitted event timings for more than kMaxSoftNavigations (100)
  // distinct navigation IDs without committing or completing any soft
  // navigations.
  for (size_t i = 0; i < SoftNavigationTracker::kMaxSoftNavigations + 10; ++i) {
    std::vector<mojom::EventTimingPtr> events;
    events.push_back(mojom::EventTiming::New());
    events.back()->performance_timeline_navigation_id =
        SoftNavigationTracker::
            kFirstSoftNavigationPerformanceTimelineNavigationId +
        i;
    events.back()->duration = base::Milliseconds(50);
    events.back()->interaction_id = i + 1;
    EXPECT_TRUE(tracker.UpdateMainFrameMetrics(
        kMainFrameToken, /*soft_navigation_metrics=*/{}, events));
  }

  // The first 100 entries (ids 2 to 101) should exist.
  EXPECT_NE(tracker.GetSoftNavigationDataForTest(2), nullptr);
  EXPECT_NE(tracker.GetSoftNavigationDataForTest(101), nullptr);

  // Beyond capacity (e.g. id 102+), GetSoftNavigationDataForTest should return
  // nullptr.
  EXPECT_EQ(tracker.GetSoftNavigationDataForTest(102), nullptr);
}

TEST(SoftNavigationTrackerTest, UncommittedNavigationsPrunedOnHigherCommit) {
  base::TimeTicks base_time = base::TimeTicks::Now();
  TestObserver observer;
  SoftNavigationTracker tracker(&observer);

  // Add event timings for navigation IDs 2, 3, 4 without committing them yet.
  for (uint64_t id = 2; id <= 4; ++id) {
    std::vector<mojom::EventTimingPtr> events;
    events.push_back(mojom::EventTiming::New());
    events.back()->performance_timeline_navigation_id = id;
    events.back()->duration = base::Milliseconds(50);
    events.back()->interaction_id = id;
    EXPECT_TRUE(tracker.UpdateMainFrameMetrics(
        kMainFrameToken, /*soft_navigation_metrics=*/{}, events));
  }

  EXPECT_NE(tracker.GetSoftNavigationDataForTest(2), nullptr);
  EXPECT_NE(tracker.GetSoftNavigationDataForTest(3), nullptr);
  EXPECT_NE(tracker.GetSoftNavigationDataForTest(4), nullptr);

  // Now, soft navigation 4 commits directly (2 and 3 were skipped/aborted).
  std::vector<mojom::SoftNavigationMetricsPtr> soft_navs;
  soft_navs.push_back(CreateSoftNavigationCommit(
      4, base::Milliseconds(100), base_time + base::Milliseconds(150)));
  EXPECT_TRUE(
      tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(soft_navs)));

  // The uncommitted buckets for 2 and 3 should be pruned.
  EXPECT_EQ(tracker.GetSoftNavigationDataForTest(2), nullptr);
  EXPECT_EQ(tracker.GetSoftNavigationDataForTest(3), nullptr);
  // Navigation 4 is active.
  EXPECT_NE(tracker.GetSoftNavigationDataForTest(4), nullptr);
  EXPECT_EQ(tracker.soft_navigation_count(), 1u);
}

}  // namespace page_load_metrics
