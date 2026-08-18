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
  void OnSoftNavigationCommit(
      const mojom::SoftNavigationMetrics& metrics) override {
    commits.push_back(metrics.Clone());
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

  std::vector<mojom::SoftNavigationMetricsPtr> commits;
  std::vector<uint64_t> completed_nav_ids;
  std::vector<CompletedSoftNavigationRecord> completed_navs;
};

}  // namespace

TEST(SoftNavigationTrackerTest, CountSoftNavigations) {
  base::TimeTicks base_time = base::TimeTicks::Now();
  TestObserver observer;
  SoftNavigationTracker tracker(&observer);
  std::vector<mojom::SoftNavigationMetricsPtr> soft_navigations;
  soft_navigations.emplace_back(mojom::SoftNavigationMetrics::New());
  soft_navigations.back()->performance_timeline_navigation_id = 2;
  soft_navigations.back()->same_document_metrics_token =
      base::UnguessableToken::Create();
  soft_navigations.back()->start_time = base::Milliseconds(100);
  soft_navigations.back()->soft_navigation_slicing_time =
      base_time + base::Milliseconds(150);
  soft_navigations.emplace_back(mojom::SoftNavigationMetrics::New());
  soft_navigations.back()->performance_timeline_navigation_id = 3;
  soft_navigations.back()->same_document_metrics_token =
      base::UnguessableToken::Create();
  soft_navigations.back()->start_time = base::Milliseconds(200);
  soft_navigations.back()->soft_navigation_slicing_time =
      base_time + base::Milliseconds(250);
  ASSERT_TRUE(tracker.UpdateMainFrameMetrics(kMainFrameToken,
                                             std::move(soft_navigations)));
  EXPECT_EQ(tracker.soft_navigation_count(), 2u);
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
    soft_navigations.push_back(mojom::SoftNavigationMetrics::New());
    soft_navigations.back()->performance_timeline_navigation_id = 2;
    soft_navigations.back()->start_time = base::Milliseconds(100);
    soft_navigations.back()->same_document_metrics_token =
        base::UnguessableToken::Create();
    EXPECT_FALSE(tracker.UpdateMainFrameMetrics(kMainFrameToken,
                                                std::move(soft_navigations)));
  }

  {
    // same_document_metrics_token missing.
    SoftNavigationTracker tracker(&observer);
    std::vector<mojom::SoftNavigationMetricsPtr> soft_navigations;
    soft_navigations.push_back(mojom::SoftNavigationMetrics::New());
    soft_navigations.back()->performance_timeline_navigation_id = 2;
    soft_navigations.back()->start_time = base::Milliseconds(100);
    soft_navigations.back()->soft_navigation_slicing_time =
        base_time + base::Milliseconds(150);
    EXPECT_FALSE(tracker.UpdateMainFrameMetrics(kMainFrameToken,
                                                std::move(soft_navigations)));
  }

  {
    // Metric with navigation_id < 2 is rejected.
    SoftNavigationTracker tracker(&observer);
    std::vector<mojom::SoftNavigationMetricsPtr> soft_navigations;
    soft_navigations.push_back(mojom::SoftNavigationMetrics::New());
    soft_navigations.back()->performance_timeline_navigation_id = 1;
    soft_navigations.back()->start_time = base::Milliseconds(100);
    soft_navigations.back()->soft_navigation_slicing_time =
        base_time + base::Milliseconds(150);
    soft_navigations.back()->same_document_metrics_token =
        base::UnguessableToken::Create();
    EXPECT_FALSE(tracker.UpdateMainFrameMetrics(kMainFrameToken,
                                                std::move(soft_navigations)));
  }

  {
    // Metric starting with navigation_id > 2 is accepted.
    SoftNavigationTracker tracker(&observer);
    std::vector<mojom::SoftNavigationMetricsPtr> soft_navigations;
    soft_navigations.push_back(mojom::SoftNavigationMetrics::New());
    soft_navigations.back()->performance_timeline_navigation_id = 3;
    soft_navigations.back()->start_time = base::Milliseconds(100);
    soft_navigations.back()->soft_navigation_slicing_time =
        base_time + base::Milliseconds(150);
    soft_navigations.back()->same_document_metrics_token =
        base::UnguessableToken::Create();
    EXPECT_TRUE(tracker.UpdateMainFrameMetrics(kMainFrameToken,
                                               std::move(soft_navigations)));
  }

  {
    // Slicing time is not monotonically increasing.
    SoftNavigationTracker tracker(&observer);
    std::vector<mojom::SoftNavigationMetricsPtr> soft_navigations;
    soft_navigations.push_back(mojom::SoftNavigationMetrics::New());
    soft_navigations.back()->performance_timeline_navigation_id = 2;
    soft_navigations.back()->start_time = base::Milliseconds(100);
    soft_navigations.back()->soft_navigation_slicing_time =
        base_time + base::Milliseconds(150);
    soft_navigations.back()->same_document_metrics_token =
        base::UnguessableToken::Create();
    soft_navigations.push_back(mojom::SoftNavigationMetrics::New());
    soft_navigations.back()->performance_timeline_navigation_id = 3;
    soft_navigations.back()->start_time = base::Milliseconds(200);
    soft_navigations.back()->soft_navigation_slicing_time =
        base_time + base::Milliseconds(140);
    soft_navigations.back()->same_document_metrics_token =
        base::UnguessableToken::Create();
    EXPECT_FALSE(tracker.UpdateMainFrameMetrics(kMainFrameToken,
                                                std::move(soft_navigations)));
  }

  {
    // The same_document_metrics_token is the same for subsequent soft
    // navigations.
    SoftNavigationTracker tracker(&observer);
    std::vector<mojom::SoftNavigationMetricsPtr> soft_navigations;
    base::UnguessableToken token = base::UnguessableToken::Create();
    soft_navigations.push_back(mojom::SoftNavigationMetrics::New());
    soft_navigations.back()->performance_timeline_navigation_id = 2;
    soft_navigations.back()->start_time = base::Milliseconds(100);
    soft_navigations.back()->soft_navigation_slicing_time =
        base_time + base::Milliseconds(150);
    soft_navigations.back()->same_document_metrics_token = token;
    EXPECT_TRUE(tracker.UpdateMainFrameMetrics(kMainFrameToken,
                                               std::move(soft_navigations)));

    std::vector<mojom::SoftNavigationMetricsPtr> soft_navigations2;
    soft_navigations2.push_back(mojom::SoftNavigationMetrics::New());
    soft_navigations2.back()->performance_timeline_navigation_id = 3;
    soft_navigations2.back()->start_time = base::Milliseconds(200);
    soft_navigations2.back()->soft_navigation_slicing_time =
        base_time + base::Milliseconds(250);
    soft_navigations2.back()->same_document_metrics_token = token;
    EXPECT_FALSE(tracker.UpdateMainFrameMetrics(kMainFrameToken,
                                                std::move(soft_navigations2)));
  }
}

TEST(SoftNavigationTrackerTest, TrackerRecoversAfterInvalidMetrics) {
  base::TimeTicks base_time = base::TimeTicks::Now();
  TestObserver observer;
  mojom::SoftNavigationMetricsPtr valid_soft_navigation =
      mojom::SoftNavigationMetrics::New();
  valid_soft_navigation->performance_timeline_navigation_id = 2;
  valid_soft_navigation->start_time = base::Milliseconds(100);
  valid_soft_navigation->soft_navigation_slicing_time =
      base_time + base::Milliseconds(150);
  valid_soft_navigation->same_document_metrics_token =
      base::UnguessableToken::Create();
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
  soft_navigations.push_back(mojom::SoftNavigationMetrics::New());
  soft_navigations.back()->performance_timeline_navigation_id = 3;
  soft_navigations.back()->start_time = base::Milliseconds(200);
  soft_navigations.back()->soft_navigation_slicing_time =
      base_time + base::Milliseconds(250);
  soft_navigations.back()->same_document_metrics_token =
      base::UnguessableToken::Create();
  EXPECT_TRUE(tracker.UpdateMainFrameMetrics(kMainFrameToken,
                                             std::move(soft_navigations)));
  EXPECT_EQ(tracker.soft_navigation_count(), 2u);
}

TEST(SoftNavigationTrackerTest,
     SimpleSoftNavsTrackingAndAggregationForInteractions) {
  base::TimeTicks base_time = base::TimeTicks::Now();
  std::vector<mojom::SoftNavigationMetricsPtr> soft_navigations;
  soft_navigations.push_back(mojom::SoftNavigationMetrics::New());
  soft_navigations.back()->performance_timeline_navigation_id = 2;
  soft_navigations.back()->same_document_metrics_token =
      base::UnguessableToken::Create();
  soft_navigations.back()->start_time = base::Milliseconds(80);
  soft_navigations.back()->soft_navigation_slicing_time =
      base_time + base::Milliseconds(100);
  soft_navigations.push_back(mojom::SoftNavigationMetrics::New());
  soft_navigations.back()->performance_timeline_navigation_id = 3;
  soft_navigations.back()->same_document_metrics_token =
      base::UnguessableToken::Create();
  soft_navigations.back()->start_time = base::Milliseconds(180);
  soft_navigations.back()->soft_navigation_slicing_time =
      base_time + base::Milliseconds(200);
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
  soft_navigations.push_back(mojom::SoftNavigationMetrics::New());
  soft_navigations.back()->performance_timeline_navigation_id = 2;
  soft_navigations.back()->same_document_metrics_token =
      base::UnguessableToken::Create();
  soft_navigations.back()->start_time = base::Milliseconds(80);
  soft_navigations.back()->soft_navigation_slicing_time =
      base_time + base::Milliseconds(100);
  soft_navigations.push_back(mojom::SoftNavigationMetrics::New());
  soft_navigations.back()->performance_timeline_navigation_id = 3;
  soft_navigations.back()->same_document_metrics_token =
      base::UnguessableToken::Create();
  soft_navigations.back()->start_time = base::Milliseconds(180);
  soft_navigations.back()->soft_navigation_slicing_time =
      base_time + base::Milliseconds(200);

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
  soft_navigations.push_back(mojom::SoftNavigationMetrics::New());
  soft_navigations.back()->performance_timeline_navigation_id = 2;
  soft_navigations.back()->same_document_metrics_token =
      base::UnguessableToken::Create();
  soft_navigations.back()->start_time = base::Milliseconds(80);
  soft_navigations.back()->soft_navigation_slicing_time =
      base_time + base::Milliseconds(100);
  soft_navigations.push_back(mojom::SoftNavigationMetrics::New());
  soft_navigations.back()->performance_timeline_navigation_id = 3;
  soft_navigations.back()->same_document_metrics_token =
      base::UnguessableToken::Create();
  soft_navigations.back()->start_time = base::Milliseconds(180);
  soft_navigations.back()->soft_navigation_slicing_time =
      base_time + base::Milliseconds(200);

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
  soft_navs_1.push_back(mojom::SoftNavigationMetrics::New());
  soft_navs_1.back()->performance_timeline_navigation_id = 2;
  soft_navs_1.back()->same_document_metrics_token =
      base::UnguessableToken::Create();
  soft_navs_1.back()->start_time = base::Milliseconds(80);
  soft_navs_1.back()->soft_navigation_slicing_time =
      base_time + base::Milliseconds(100);
  EXPECT_TRUE(
      tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(soft_navs_1)));
  EXPECT_EQ(observer.commits.size(), 1u);
  EXPECT_EQ(observer.commits[0]->performance_timeline_navigation_id, 2u);
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
  soft_navs_2.push_back(mojom::SoftNavigationMetrics::New());
  soft_navs_2.back()->performance_timeline_navigation_id = 3;
  soft_navs_2.back()->same_document_metrics_token =
      base::UnguessableToken::Create();
  soft_navs_2.back()->start_time = base::Milliseconds(180);
  soft_navs_2.back()->soft_navigation_slicing_time =
      base_time + base::Milliseconds(200);
  EXPECT_TRUE(
      tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(soft_navs_2)));

  // With Soft Nav 2 committed, Soft Nav 1 is now completed.
  ASSERT_EQ(observer.completed_nav_ids.size(), 1u);
  EXPECT_EQ(observer.completed_nav_ids[0], 2u);
  ASSERT_EQ(observer.commits.size(), 2u);
  EXPECT_EQ(observer.commits[1]->performance_timeline_navigation_id, 3u);

  // Soft Nav 1 has been completed and pruned from `navigations_`.
  EXPECT_EQ(tracker.GetSoftNavigationDataForTest(2), nullptr);

  // Late-arriving events for the already completed Soft Nav 1 are safely
  // ignored.
  std::vector<mojom::EventTimingPtr> late_events;
  late_events.push_back(mojom::EventTiming::New());
  late_events.back()->processing_start = base_time + base::Milliseconds(180);
  late_events.back()->duration = base::Milliseconds(30);
  late_events.back()->interaction_id = 3;
  late_events.back()->performance_timeline_navigation_id = 2;
  EXPECT_TRUE(tracker.UpdateMainFrameMetrics(
      kMainFrameToken, /*soft_navigation_metrics=*/{}, late_events));
  EXPECT_EQ(tracker.GetSoftNavigationDataForTest(2), nullptr);

  // Events for active Soft Nav 2 (nav_id = 3) are recorded.
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

  // Finalize all active navigations and take completed navigations on flush.
  tracker.CompleteActiveNavigationAndFlush();
  ASSERT_EQ(observer.completed_nav_ids.size(), 2u);
  EXPECT_EQ(observer.completed_nav_ids[1], 3u);
  EXPECT_EQ(tracker.GetSoftNavigationDataForTest(3), nullptr);
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
  soft_navs.push_back(mojom::SoftNavigationMetrics::New());
  soft_navs.back()->performance_timeline_navigation_id = 4;
  soft_navs.back()->same_document_metrics_token =
      base::UnguessableToken::Create();
  soft_navs.back()->start_time = base::Milliseconds(100);
  soft_navs.back()->soft_navigation_slicing_time =
      base_time + base::Milliseconds(150);
  EXPECT_TRUE(
      tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(soft_navs)));

  // The uncommitted buckets for 2 and 3 should be pruned.
  EXPECT_EQ(tracker.GetSoftNavigationDataForTest(2), nullptr);
  EXPECT_EQ(tracker.GetSoftNavigationDataForTest(3), nullptr);
  // Navigation 4 is active.
  EXPECT_NE(tracker.GetSoftNavigationDataForTest(4), nullptr);
  EXPECT_EQ(observer.commits.size(), 1u);
  EXPECT_EQ(observer.commits[0]->performance_timeline_navigation_id, 4u);
}

}  // namespace page_load_metrics
