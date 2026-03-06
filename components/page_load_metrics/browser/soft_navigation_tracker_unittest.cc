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

TEST(SoftNavigationTrackerTest, CountSoftNavigations) {
  SoftNavigationTracker tracker;
  std::vector<mojom::SoftNavigationMetricsPtr> soft_navigations;
  soft_navigations.emplace_back(mojom::SoftNavigationMetrics::New());
  soft_navigations.back()->soft_navigation_offset = 1;
  soft_navigations.back()->same_document_metrics_token =
      base::UnguessableToken::Create();
  soft_navigations.back()->soft_navigation_slicing_time =
      base::TimeTicks() + base::Milliseconds(100);
  soft_navigations.emplace_back(mojom::SoftNavigationMetrics::New());
  soft_navigations.back()->soft_navigation_offset = 2;
  soft_navigations.back()->same_document_metrics_token =
      base::UnguessableToken::Create();
  soft_navigations.back()->soft_navigation_slicing_time =
      base::TimeTicks() + base::Milliseconds(200);
  ASSERT_TRUE(tracker.UpdateAndValidateMetrics(std::move(soft_navigations)));
  EXPECT_EQ(tracker.soft_navigation_count(), 2u);
}

TEST(SoftNavigationTrackerTest,
     UpdateAndValidateMetricsValidatesIncomingMetrics) {
  base::TimeTicks base_time = base::TimeTicks::Now();

  {
    // Empty metric is rejected.
    SoftNavigationTracker tracker;
    std::vector<mojom::SoftNavigationMetricsPtr> soft_navigations;
    soft_navigations.push_back(mojom::SoftNavigationMetrics::New());
    EXPECT_FALSE(tracker.UpdateAndValidateMetrics(std::move(soft_navigations)));
  }

  {
    // Slicing time missing.
    SoftNavigationTracker tracker;
    std::vector<mojom::SoftNavigationMetricsPtr> soft_navigations;
    soft_navigations.push_back(mojom::SoftNavigationMetrics::New());
    soft_navigations.back()->soft_navigation_offset = 1;
    soft_navigations.back()->same_document_metrics_token =
        base::UnguessableToken::Create();
    EXPECT_FALSE(tracker.UpdateAndValidateMetrics(std::move(soft_navigations)));
  }

  {
    // same_document_metrics_token missing.
    SoftNavigationTracker tracker;
    std::vector<mojom::SoftNavigationMetricsPtr> soft_navigations;
    soft_navigations.push_back(mojom::SoftNavigationMetrics::New());
    soft_navigations.back()->soft_navigation_offset = 1;
    soft_navigations.back()->soft_navigation_slicing_time =
        base_time + base::Milliseconds(100);
    EXPECT_FALSE(tracker.UpdateAndValidateMetrics(std::move(soft_navigations)));
  }

  {
    // First metric starts with offset 2 (instead of 1).
    SoftNavigationTracker tracker;
    std::vector<mojom::SoftNavigationMetricsPtr> soft_navigations;
    soft_navigations.push_back(mojom::SoftNavigationMetrics::New());
    soft_navigations.back()->soft_navigation_offset = 2;
    soft_navigations.back()->soft_navigation_slicing_time =
        base_time + base::Milliseconds(100);
    soft_navigations.back()->same_document_metrics_token =
        base::UnguessableToken::Create();
    EXPECT_FALSE(tracker.UpdateAndValidateMetrics(std::move(soft_navigations)));
  }

  {
    // Slicing time is not monotonically increasing.
    SoftNavigationTracker tracker;
    std::vector<mojom::SoftNavigationMetricsPtr> soft_navigations;
    soft_navigations.push_back(mojom::SoftNavigationMetrics::New());
    soft_navigations.back()->soft_navigation_offset = 1;
    soft_navigations.back()->soft_navigation_slicing_time =
        base_time + base::Milliseconds(100);
    soft_navigations.back()->same_document_metrics_token =
        base::UnguessableToken::Create();
    soft_navigations.push_back(mojom::SoftNavigationMetrics::New());
    soft_navigations.back()->soft_navigation_offset = 2;
    soft_navigations.back()->soft_navigation_slicing_time =
        base_time + base::Milliseconds(90);
    soft_navigations.back()->same_document_metrics_token =
        base::UnguessableToken::Create();
    EXPECT_FALSE(tracker.UpdateAndValidateMetrics(std::move(soft_navigations)));
  }

  {
    // The same_document_metrics_token is the same for a subsequent soft
    // navigations.
    SoftNavigationTracker tracker;
    std::vector<mojom::SoftNavigationMetricsPtr> soft_navigations;
    base::UnguessableToken token = base::UnguessableToken::Create();
    soft_navigations.push_back(mojom::SoftNavigationMetrics::New());
    soft_navigations.back()->soft_navigation_offset = 1;
    soft_navigations.back()->soft_navigation_slicing_time =
        base_time + base::Milliseconds(100);
    soft_navigations.back()->same_document_metrics_token = token;
    EXPECT_TRUE(tracker.UpdateAndValidateMetrics(std::move(soft_navigations)));

    tracker.AdvanceToNextSoftNavigation();

    std::vector<mojom::SoftNavigationMetricsPtr> soft_navigations2;
    soft_navigations2.push_back(mojom::SoftNavigationMetrics::New());
    soft_navigations2.back()->soft_navigation_offset = 2;
    soft_navigations2.back()->soft_navigation_slicing_time =
        base_time + base::Milliseconds(200);
    soft_navigations2.back()->same_document_metrics_token = token;
    EXPECT_FALSE(
        tracker.UpdateAndValidateMetrics(std::move(soft_navigations2)));
  }
}

TEST(SoftNavigationTrackerTest, TrackerRecoversAfterInvalidMetrics) {
  base::TimeTicks base_time = base::TimeTicks::Now();
  mojom::SoftNavigationMetricsPtr valid_soft_navigation =
      mojom::SoftNavigationMetrics::New();
  valid_soft_navigation->soft_navigation_offset = 1;
  valid_soft_navigation->soft_navigation_slicing_time =
      base_time + base::Milliseconds(100);
  valid_soft_navigation->same_document_metrics_token =
      base::UnguessableToken::Create();
  {
    // Verify that this is a valid soft navigation.
    SoftNavigationTracker tracker;
    std::vector<mojom::SoftNavigationMetricsPtr> soft_navigations;
    soft_navigations.push_back(valid_soft_navigation->Clone());
    EXPECT_TRUE(tracker.UpdateAndValidateMetrics(std::move(soft_navigations)));
  }

  SoftNavigationTracker tracker;
  std::vector<mojom::SoftNavigationMetricsPtr> soft_navigations;
  // This first metric is valid, but the second one is not (it's empty).
  // Therefore, the tracker should not accept any metrics, and
  // UpdateAndValidateMetrics should return false.
  soft_navigations.push_back(valid_soft_navigation->Clone());
  soft_navigations.push_back(mojom::SoftNavigationMetrics::New());
  EXPECT_FALSE(tracker.UpdateAndValidateMetrics(std::move(soft_navigations)));
  EXPECT_FALSE(tracker.HasNextSoftNavigation());
  // But, after this, the tracker should be in a good state, so we can test
  // that it accepts metrics in the next call.
  soft_navigations.clear();
  soft_navigations.push_back(valid_soft_navigation->Clone());
  soft_navigations.push_back(mojom::SoftNavigationMetrics::New());
  soft_navigations.back()->soft_navigation_offset = 2;
  soft_navigations.back()->soft_navigation_slicing_time =
      base_time + base::Milliseconds(200);
  soft_navigations.back()->same_document_metrics_token =
      base::UnguessableToken::Create();
  EXPECT_TRUE(tracker.UpdateAndValidateMetrics(std::move(soft_navigations)));
  EXPECT_TRUE(tracker.HasNextSoftNavigation());
}

TEST(SoftNavigationTrackerTest,
     SimpleSoftNavsTrackingAndAggregationForInteractions) {
  // Shows how a single list of soft navigations is used to slice
  // a single list of latencies into normalizations (aggregations).
  // These data would come from the renderer in a single call.
  std::vector<mojom::SoftNavigationMetricsPtr> soft_navigations;
  soft_navigations.push_back(mojom::SoftNavigationMetrics::New());
  soft_navigations.back()->soft_navigation_offset = 1;
  soft_navigations.back()->same_document_metrics_token =
      base::UnguessableToken::Create();
  soft_navigations.back()->soft_navigation_slicing_time =
      base::TimeTicks() + base::Milliseconds(100);
  soft_navigations.push_back(mojom::SoftNavigationMetrics::New());
  soft_navigations.back()->soft_navigation_offset = 2;
  soft_navigations.back()->same_document_metrics_token =
      base::UnguessableToken::Create();
  soft_navigations.back()->soft_navigation_slicing_time =
      base::TimeTicks() + base::Milliseconds(200);
  std::vector<mojom::EventTimingPtr> latencies;
  // Before the first soft navigation, there are two user interactions.
  latencies.push_back(mojom::EventTiming::New());
  latencies.back()->start_time = base::TimeTicks() + base::Milliseconds(50);
  latencies.back()->duration = base::Milliseconds(10);
  latencies.back()->interaction_id = 1;
  latencies.push_back(mojom::EventTiming::New());
  latencies.back()->start_time = base::TimeTicks() + base::Milliseconds(80);
  latencies.back()->duration = base::Milliseconds(20);
  latencies.back()->interaction_id = 2;
  // After the first soft navigation, there is one user interaction.
  latencies.push_back(mojom::EventTiming::New());
  latencies.back()->start_time = base::TimeTicks() + base::Milliseconds(150);
  latencies.back()->duration = base::Milliseconds(30);
  latencies.back()->interaction_id = 3;
  // After the second soft navigation, there is another user interaction.
  latencies.push_back(mojom::EventTiming::New());
  latencies.back()->start_time = base::TimeTicks() + base::Milliseconds(250);
  latencies.back()->duration = base::Milliseconds(20);
  latencies.back()->interaction_id = 4;

  InteractionToNextPaintCalculator calculator;
  SoftNavigationTracker tracker;
  ASSERT_TRUE(tracker.UpdateAndValidateMetrics(std::move(soft_navigations)));
  EXPECT_EQ(tracker.soft_navigation_count(), 2u);  // 2 soft navigations total.

  // Process data before the first soft navigation.
  base::span<const mojom::EventTimingPtr> latencies_span(latencies);
  EXPECT_EQ(tracker.Process(&latencies_span, &calculator), 2u);
  EXPECT_EQ(calculator.num_user_interactions(), 2u);
  ASSERT_TRUE(calculator.worst_latency().has_value());
  EXPECT_EQ(calculator.worst_latency()->max_event.duration,
            base::Milliseconds(20));
  ASSERT_TRUE(calculator.ApproximateHighPercentile().has_value());
  EXPECT_EQ(calculator.ApproximateHighPercentile()->max_event.duration,
            base::Milliseconds(20));

  // Process data during the first soft navigation.
  EXPECT_TRUE(tracker.HasNextSoftNavigation());
  calculator.ClearEventTimings();
  tracker.AdvanceToNextSoftNavigation();
  EXPECT_EQ(tracker.Process(&latencies_span, &calculator), 1u);
  EXPECT_EQ(calculator.num_user_interactions(), 1u);
  ASSERT_TRUE(calculator.worst_latency().has_value());
  EXPECT_EQ(calculator.worst_latency()->max_event.duration,
            base::Milliseconds(30));
  ASSERT_TRUE(calculator.ApproximateHighPercentile().has_value());
  EXPECT_EQ(calculator.ApproximateHighPercentile()->max_event.duration,
            base::Milliseconds(30));

  // Process data during the second soft navigation.
  EXPECT_TRUE(tracker.HasNextSoftNavigation());
  tracker.AdvanceToNextSoftNavigation();
  calculator.ClearEventTimings();
  EXPECT_EQ(tracker.Process(&latencies_span, &calculator), 1u);
  EXPECT_EQ(calculator.num_user_interactions(), 1u);
  ASSERT_TRUE(calculator.worst_latency().has_value());
  EXPECT_EQ(calculator.worst_latency()->max_event.duration,
            base::Milliseconds(20));
  ASSERT_TRUE(calculator.ApproximateHighPercentile().has_value());
  EXPECT_EQ(calculator.ApproximateHighPercentile()->max_event.duration,
            base::Milliseconds(20));

  // We're done, no more soft navigations.
  EXPECT_FALSE(tracker.HasNextSoftNavigation());
}

TEST(SoftNavigationTrackerTest,
     SimpleSoftNavsTrackingAndAggregationForLayoutShifts) {
  base::TimeTicks base_time = base::TimeTicks::Now();
  // Two soft navigations, at 100ms and 200ms.
  std::vector<mojom::SoftNavigationMetricsPtr> soft_navigations;
  soft_navigations.push_back(mojom::SoftNavigationMetrics::New());
  soft_navigations.back()->soft_navigation_offset = 1;
  soft_navigations.back()->same_document_metrics_token =
      base::UnguessableToken::Create();
  soft_navigations.back()->soft_navigation_slicing_time =
      base_time + base::Milliseconds(100);
  soft_navigations.push_back(mojom::SoftNavigationMetrics::New());
  soft_navigations.back()->soft_navigation_offset = 2;
  soft_navigations.back()->same_document_metrics_token =
      base::UnguessableToken::Create();
  soft_navigations.back()->soft_navigation_slicing_time =
      base_time + base::Milliseconds(200);

  // Before the first soft navigation, there are two layout shifts, with
  // scores 0.1 and 0.21.
  std::vector<mojom::LayoutShiftPtr> layout_shifts;
  layout_shifts.push_back(mojom::LayoutShift::New());
  layout_shifts.back()->layout_shift_time = base_time + base::Milliseconds(50);
  layout_shifts.back()->layout_shift_score = 0.1;
  layout_shifts.push_back(mojom::LayoutShift::New());
  layout_shifts.back()->layout_shift_time = base_time + base::Milliseconds(80);
  layout_shifts.back()->layout_shift_score = 0.21;
  // After the first soft navigation, there's a layout shift with score 0.3.
  layout_shifts.push_back(mojom::LayoutShift::New());
  layout_shifts.back()->layout_shift_time = base_time + base::Milliseconds(150);
  layout_shifts.back()->layout_shift_score = 0.3;
  // After the second soft navigation, there's a layout shift with score 0.4.
  layout_shifts.push_back(mojom::LayoutShift::New());
  layout_shifts.back()->layout_shift_time = base_time + base::Milliseconds(250);
  layout_shifts.back()->layout_shift_score = 0.4;

  SoftNavigationTracker tracker;
  ASSERT_TRUE(tracker.UpdateAndValidateMetrics(std::move(soft_navigations)));
  EXPECT_EQ(tracker.soft_navigation_count(), 2u);
  EXPECT_TRUE(tracker.HasNextSoftNavigation());

  LayoutShiftNormalization normalization;
  base::span<const mojom::LayoutShiftPtr> layout_shifts_span(layout_shifts);

  EXPECT_EQ(tracker.Process(&layout_shifts_span, &normalization), 2u);
  // 0.1 + 0.21 = 0.31 - the CLS score *before* the first soft navigation.
  EXPECT_FLOAT_EQ(normalization.normalized_cls_data()
                      .session_windows_gap1000ms_max5000ms_max_cls,
                  0.31f);

  // Now process the first soft navigation.
  EXPECT_TRUE(tracker.HasNextSoftNavigation());
  normalization.ClearAllLayoutShifts();
  tracker.AdvanceToNextSoftNavigation();
  EXPECT_EQ(tracker.Process(&layout_shifts_span, &normalization), 1u);
  // 0.3 is the CLS score *during* the first soft navigation.
  EXPECT_FLOAT_EQ(normalization.normalized_cls_data()
                      .session_windows_gap1000ms_max5000ms_max_cls,
                  0.3f);

  // Now process the second soft navigation.
  EXPECT_TRUE(tracker.HasNextSoftNavigation());
  tracker.AdvanceToNextSoftNavigation();
  normalization.ClearAllLayoutShifts();
  EXPECT_EQ(tracker.Process(&layout_shifts_span, &normalization), 1u);
  EXPECT_FLOAT_EQ(normalization.normalized_cls_data()
                      .session_windows_gap1000ms_max5000ms_max_cls,
                  0.4f);
  // No more soft navigations.
  EXPECT_FALSE(tracker.HasNextSoftNavigation());
}

TEST(SoftNavigationTrackerTest,
     SimpleSoftNavsTrackingAndAggregationForLargestContentfulPaint) {
  base::TimeTicks base_time = base::TimeTicks::Now();

  // Two soft navigations, at offsets 1 and 2.
  std::vector<mojom::SoftNavigationMetricsPtr> soft_navigations;
  soft_navigations.push_back(mojom::SoftNavigationMetrics::New());
  soft_navigations.back()->soft_navigation_offset = 1;
  soft_navigations.back()->same_document_metrics_token =
      base::UnguessableToken::Create();
  soft_navigations.back()->soft_navigation_slicing_time =
      base_time + base::Milliseconds(100);
  soft_navigations.push_back(mojom::SoftNavigationMetrics::New());
  soft_navigations.back()->soft_navigation_offset = 2;
  soft_navigations.back()->same_document_metrics_token =
      base::UnguessableToken::Create();
  soft_navigations.back()->soft_navigation_slicing_time =
      base_time + base::Milliseconds(200);

  std::vector<mojom::LargestContentfulPaintTimingPtr> lcps;

  // Before the first soft navigation (offset = 0), there is one text LCP.
  lcps.push_back(mojom::LargestContentfulPaintTiming::New());
  lcps.back()->largest_text_paint = base::Milliseconds(50);
  lcps.back()->largest_text_paint_size = 400;
  lcps.back()->type = 0;
  lcps.back()->soft_navigation_offset = 0;
  lcps.back()->resource_load_timings = mojom::LcpResourceLoadTimings::New();

  // Between the first and second soft navigation (offset = 1), we see a smaller
  // text LCP, followed by a larger image LCP.
  lcps.push_back(mojom::LargestContentfulPaintTiming::New());
  lcps.back()->largest_text_paint = base::Milliseconds(120);
  lcps.back()->largest_text_paint_size = 50;
  lcps.back()->type = 0;
  lcps.back()->soft_navigation_offset = 1;
  lcps.back()->resource_load_timings = mojom::LcpResourceLoadTimings::New();

  lcps.push_back(mojom::LargestContentfulPaintTiming::New());
  lcps.back()->largest_image_paint = base::Milliseconds(150);
  lcps.back()->largest_image_paint_size = 375;
  lcps.back()->type = 0;
  lcps.back()->soft_navigation_offset = 1;
  lcps.back()->resource_load_timings = mojom::LcpResourceLoadTimings::New();

  // After the second soft navigation (offset = 2), we see an image LCP.
  lcps.push_back(mojom::LargestContentfulPaintTiming::New());
  lcps.back()->largest_image_paint = base::Milliseconds(240);
  lcps.back()->largest_image_paint_size = 800;
  lcps.back()->type = 0;
  lcps.back()->soft_navigation_offset = 2;
  lcps.back()->resource_load_timings = mojom::LcpResourceLoadTimings::New();

  SoftNavigationTracker tracker;
  ASSERT_TRUE(tracker.UpdateAndValidateMetrics(std::move(soft_navigations)));
  EXPECT_EQ(tracker.soft_navigation_count(), 2u);
  EXPECT_TRUE(tracker.HasNextSoftNavigation());

  ContentfulPaint lcp_candidate(
      /*in_main_frame=*/true, blink::LargestContentfulPaintType::kNone);
  base::span<const mojom::LargestContentfulPaintTimingPtr> lcps_span(lcps);

  EXPECT_EQ(tracker.Process(&lcps_span, &lcp_candidate), 1u);
  // Before first soft nav, largest is the text LCP with size 400.
  EXPECT_EQ(lcp_candidate.MergeTextAndImageTiming().Size(), 400u);
  EXPECT_EQ(lcp_candidate.MergeTextAndImageTiming().Time().value(),
            base::Milliseconds(50));

  // Now process the first soft navigation.
  EXPECT_TRUE(tracker.HasNextSoftNavigation());
  lcp_candidate.Clear();
  tracker.AdvanceToNextSoftNavigation();
  EXPECT_EQ(tracker.Process(&lcps_span, &lcp_candidate), 2u);
  // During first soft nav, largest is the image LCP with size 375.
  EXPECT_EQ(lcp_candidate.MergeTextAndImageTiming().Size(), 375u);
  EXPECT_EQ(lcp_candidate.MergeTextAndImageTiming().Time().value(),
            base::Milliseconds(150));

  // Now process the second soft navigation.
  EXPECT_TRUE(tracker.HasNextSoftNavigation());
  tracker.AdvanceToNextSoftNavigation();
  lcp_candidate.Clear();
  EXPECT_EQ(tracker.Process(&lcps_span, &lcp_candidate), 1u);
  // After second soft nav, largest is the image LCP with size 800.
  EXPECT_EQ(lcp_candidate.MergeTextAndImageTiming().Size(), 800u);
  EXPECT_EQ(lcp_candidate.MergeTextAndImageTiming().Time().value(),
            base::Milliseconds(240));

  // No more soft navigations.
  EXPECT_FALSE(tracker.HasNextSoftNavigation());
}

TEST(SoftNavigationTrackerTest, IncrementalSoftNavigationUpdates) {
  SoftNavigationTracker tracker;
  InteractionToNextPaintCalculator calculator;

  // Step 1: Soft Nav 1 arrives.
  // One event at 50ms (before Soft Nav 1).
  std::vector<mojom::SoftNavigationMetricsPtr> soft_navs_1;
  soft_navs_1.push_back(mojom::SoftNavigationMetrics::New());
  soft_navs_1.back()->soft_navigation_offset = 1;
  soft_navs_1.back()->same_document_metrics_token =
      base::UnguessableToken::Create();
  soft_navs_1.back()->soft_navigation_slicing_time =
      base::TimeTicks() + base::Milliseconds(100);
  tracker.UpdateAndValidateMetrics(std::move(soft_navs_1));

  std::vector<mojom::EventTimingPtr> events_1;
  events_1.push_back(mojom::EventTiming::New());
  events_1.back()->start_time = base::TimeTicks() + base::Milliseconds(50);
  events_1.back()->duration = base::Milliseconds(10);
  events_1.back()->interaction_id = 1;

  base::span<const mojom::EventTimingPtr> events_span_1(events_1);
  EXPECT_EQ(tracker.Process(&events_span_1, &calculator), 1u);

  // Event at 50ms should be processed as it is <= 100ms.
  EXPECT_EQ(calculator.num_user_interactions(), 1u);
  EXPECT_EQ(calculator.worst_latency()->max_event.duration,
            base::Milliseconds(10));

  EXPECT_TRUE(tracker.HasNextSoftNavigation());
  tracker.AdvanceToNextSoftNavigation();
  calculator.ClearEventTimings();

  // Step 2: Events for Soft Nav 1 arrive.
  // Event at 150ms.
  std::vector<mojom::EventTimingPtr> events_2;
  events_2.push_back(mojom::EventTiming::New());
  events_2.back()->start_time = base::TimeTicks() + base::Milliseconds(150);
  events_2.back()->duration = base::Milliseconds(20);
  events_2.back()->interaction_id = 2;

  base::span<const mojom::EventTimingPtr> events_span_2(events_2);
  EXPECT_EQ(tracker.Process(&events_span_2, &calculator), 1u);

  // Event at 150ms should be processed as queue is empty (open-ended interval).
  EXPECT_EQ(calculator.num_user_interactions(), 1u);
  EXPECT_EQ(calculator.worst_latency()->max_event.duration,
            base::Milliseconds(20));

  // Step 3: Soft Nav 2 arrives.
  // Event at 180ms (belongs to Soft Nav 1) and 250ms (belongs to Soft Nav 2).
  std::vector<mojom::SoftNavigationMetricsPtr> soft_navs_2;
  soft_navs_2.push_back(mojom::SoftNavigationMetrics::New());
  soft_navs_2.back()->soft_navigation_offset = 2;
  soft_navs_2.back()->same_document_metrics_token =
      base::UnguessableToken::Create();
  soft_navs_2.back()->soft_navigation_slicing_time =
      base::TimeTicks() + base::Milliseconds(200);
  tracker.UpdateAndValidateMetrics(std::move(soft_navs_2));

  std::vector<mojom::EventTimingPtr> events_3;
  events_3.push_back(mojom::EventTiming::New());
  events_3.back()->start_time = base::TimeTicks() + base::Milliseconds(180);
  events_3.back()->duration = base::Milliseconds(30);
  events_3.back()->interaction_id = 3;
  events_3.push_back(mojom::EventTiming::New());
  events_3.back()->start_time = base::TimeTicks() + base::Milliseconds(250);
  events_3.back()->duration = base::Milliseconds(40);
  events_3.back()->interaction_id = 4;

  base::span<const mojom::EventTimingPtr> events_span_3(events_3);
  EXPECT_EQ(tracker.Process(&events_span_3, &calculator), 1u);

  // Event at 180ms should be processed (<= 200ms).
  // Calculator accumulates 180ms event.
  // Previous 150ms event is also in the calculator, as we didn't clear it.
  // So count should be 2.
  EXPECT_EQ(calculator.num_user_interactions(), 2u);
  // Max duration is 30ms (from 180ms event) vs 20ms (from 150ms event).
  EXPECT_EQ(calculator.worst_latency()->max_event.duration,
            base::Milliseconds(30));

  EXPECT_TRUE(tracker.HasNextSoftNavigation());
  tracker.AdvanceToNextSoftNavigation();
  calculator.ClearEventTimings();

  // Step 4: Process remaining events for Soft Nav 2.
  // Event at 250ms (remaining in events_span_3).
  EXPECT_EQ(tracker.Process(&events_span_3, &calculator), 1u);

  EXPECT_EQ(calculator.num_user_interactions(), 1u);
  EXPECT_EQ(calculator.worst_latency()->max_event.duration,
            base::Milliseconds(40));
}

}  // namespace page_load_metrics
