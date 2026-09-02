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
const content::GlobalRenderFrameHostToken kSubFrameToken{
    2, blink::LocalFrameToken()};

struct CompletedSoftNavigationRecord {
  uint64_t navigation_id = 0;
  InteractionToNextPaintCalculator inp_calculator;
  LayoutShiftNormalization cls_calculator;
  ContentfulPaintTimingInfo lcp;
  std::optional<base::TimeDelta> first_background_time;
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
          .first_background_time = data.first_background_time,
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
     FlushDoesNotDispatchFcpOrCompleteForIncompleteNavigations) {
  base::TimeTicks base_time = base::TimeTicks::Now();
  TestObserver observer;
  SoftNavigationTracker tracker(&observer);

  // Nav 2 commits without FCP.
  std::vector<mojom::SoftNavigationMetricsPtr> nav_commits;
  nav_commits.push_back(CreateSoftNavigationCommit(
      2, base::Milliseconds(100), base_time + base::Milliseconds(150)));
  ASSERT_TRUE(
      tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(nav_commits)));
  EXPECT_EQ(tracker.soft_navigation_count(), 1u);
  EXPECT_TRUE(observer.fcps.empty());
  EXPECT_TRUE(observer.completed_nav_ids.empty());

  // Teardown / flush does not dispatch FCP or completion for unrendered nav 2.
  tracker.CompleteActiveNavigationAndFlush();
  EXPECT_TRUE(observer.fcps.empty());
  EXPECT_TRUE(observer.completed_nav_ids.empty());
}

TEST(SoftNavigationTrackerTest,
     FlushDoesNotDispatchFcpForBlockedOutOfOrderNavigations) {
  base::TimeTicks base_time = base::TimeTicks::Now();
  TestObserver observer;
  SoftNavigationTracker tracker(&observer);

  // Nav 2 and Nav 3 commit without FCP.
  std::vector<mojom::SoftNavigationMetricsPtr> nav_commits;
  nav_commits.push_back(CreateSoftNavigationCommit(
      2, base::Milliseconds(100), base_time + base::Milliseconds(150)));
  nav_commits.push_back(CreateSoftNavigationCommit(
      3, base::Milliseconds(200), base_time + base::Milliseconds(250)));
  ASSERT_TRUE(
      tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(nav_commits)));

  // Nav 3 receives FCP out of order while Nav 2 has not received FCP.
  std::vector<mojom::SoftNavigationMetricsPtr> nav3_fcp;
  nav3_fcp.push_back(CreateSoftNavigationFcpUpdate(3, base::Milliseconds(220)));
  ASSERT_TRUE(
      tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(nav3_fcp)));
  EXPECT_TRUE(observer.fcps.empty());
  EXPECT_TRUE(observer.completed_nav_ids.empty());

  // Teardown / flush must not fire FCP for Nav 3 or complete either navigation.
  tracker.CompleteActiveNavigationAndFlush();
  EXPECT_TRUE(observer.fcps.empty());
  EXPECT_TRUE(observer.completed_nav_ids.empty());
}

TEST(SoftNavigationTrackerTest,
     FlushOnlyCompletesNavigationsWithPreviouslyReportedFcp) {
  base::TimeTicks base_time = base::TimeTicks::Now();
  TestObserver observer;
  SoftNavigationTracker tracker(&observer);

  // Nav 2 commits with FCP -> FCP dispatched immediately.
  std::vector<mojom::SoftNavigationMetricsPtr> nav2;
  nav2.push_back(CreateSoftNavigationCommit(
      2, base::Milliseconds(100), base_time + base::Milliseconds(150),
      base::UnguessableToken::Create(),
      blink::mojom::NavigationTypeForNavigationApi::kPush,
      base::Milliseconds(120)));
  ASSERT_TRUE(tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(nav2)));
  EXPECT_EQ(observer.fcps.size(), 1u);
  EXPECT_TRUE(observer.completed_nav_ids.empty());

  // Nav 3 commits without FCP.
  std::vector<mojom::SoftNavigationMetricsPtr> nav3_commit;
  nav3_commit.push_back(CreateSoftNavigationCommit(
      3, base::Milliseconds(200), base_time + base::Milliseconds(250)));
  ASSERT_TRUE(tracker.UpdateMainFrameMetrics(kMainFrameToken,
                                            std::move(nav3_commit)));
  EXPECT_EQ(observer.fcps.size(), 1u);
  EXPECT_TRUE(observer.completed_nav_ids.empty());

  // Teardown / flush completes Nav 2 without re-firing FCP, and ignores Nav 3.
  tracker.CompleteActiveNavigationAndFlush();
  EXPECT_EQ(observer.fcps.size(), 1u);
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

  // Step 5: Events for Soft Nav 2 (nav_id = 3) are recorded.
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
  base::TimeTicks base_time = base::TimeTicks::Now();
  TestObserver observer;
  SoftNavigationTracker tracker(&observer);

  // Send commits for more than kMaxSoftNavigations (100) distinct navigation
  // IDs without completing them.
  std::vector<mojom::SoftNavigationMetricsPtr> soft_navs;
  for (size_t i = 0; i < SoftNavigationTracker::kMaxSoftNavigations + 10; ++i) {
    soft_navs.push_back(CreateSoftNavigationCommit(
        SoftNavigationTracker::
                kFirstSoftNavigationPerformanceTimelineNavigationId +
            i,
        base::Milliseconds(100 + i * 10),
        base_time + base::Milliseconds(100 + i * 10)));
  }
  EXPECT_TRUE(
      tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(soft_navs)));

  // The first 100 entries (ids 2 to 101) should exist.
  EXPECT_NE(tracker.GetSoftNavigationDataForTest(2), nullptr);
  EXPECT_NE(tracker.GetSoftNavigationDataForTest(101), nullptr);

  // Beyond capacity (e.g. id 102+), GetSoftNavigationDataForTest should
  // return nullptr.
  EXPECT_EQ(tracker.GetSoftNavigationDataForTest(102), nullptr);
}

TEST(SoftNavigationTrackerTest, EventsForUncommittedNavIgnored) {
  base::TimeTicks base_time = base::TimeTicks::Now();
  TestObserver observer;
  SoftNavigationTracker tracker(&observer);

  // Add event timings for navigation IDs 2, 3, 4 without committing them yet.
  std::vector<mojom::EventTimingPtr> events;
  for (uint64_t id = 2; id <= 4; ++id) {
    events.push_back(mojom::EventTiming::New());
    events.back()->performance_timeline_navigation_id = id;
    events.back()->duration = base::Milliseconds(50);
    events.back()->interaction_id = id;
  }
  EXPECT_TRUE(tracker.UpdateMainFrameMetrics(
      kMainFrameToken, /*soft_navigation_metrics=*/{}, events));

  // No uncommitted placeholder buckets should be created.
  EXPECT_EQ(tracker.GetSoftNavigationDataForTest(2), nullptr);
  EXPECT_EQ(tracker.GetSoftNavigationDataForTest(3), nullptr);
  EXPECT_EQ(tracker.GetSoftNavigationDataForTest(4), nullptr);

  // Now, soft navigation 4 commits directly (2 and 3 were skipped/aborted).
  std::vector<mojom::SoftNavigationMetricsPtr> soft_navs;
  soft_navs.push_back(CreateSoftNavigationCommit(
      4, base::Milliseconds(100), base_time + base::Milliseconds(150)));
  EXPECT_TRUE(
      tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(soft_navs)));

  // Navigation 2 and 3 remain uncreated, while navigation 4 is created.
  EXPECT_EQ(tracker.GetSoftNavigationDataForTest(2), nullptr);
  EXPECT_EQ(tracker.GetSoftNavigationDataForTest(3), nullptr);
  EXPECT_NE(tracker.GetSoftNavigationDataForTest(4), nullptr);
  EXPECT_EQ(tracker.soft_navigation_count(), 1u);
}

TEST(SoftNavigationTrackerTest, ForegroundAndBackgroundTracking) {
  base::TimeTicks base_time = base::TimeTicks::Now();
  TestObserver observer;
  SoftNavigationTracker tracker(&observer);

  // Soft nav 2 starts in foreground at 100ms.
  std::vector<mojom::SoftNavigationMetricsPtr> soft_navs;
  soft_navs.push_back(CreateSoftNavigationCommit(
      2, base::Milliseconds(100), base_time + base::Milliseconds(100),
      base::UnguessableToken::Create(),
      blink::mojom::NavigationTypeForNavigationApi::kPush,
      base::Milliseconds(150)));
  EXPECT_TRUE(
      tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(soft_navs)));

  // Page goes to background at 500ms.
  tracker.OnHidden(base::Milliseconds(500));

  // Soft nav 3 commits in background at 600ms.
  soft_navs.clear();
  soft_navs.push_back(CreateSoftNavigationCommit(
      3, base::Milliseconds(600), base_time + base::Milliseconds(600),
      base::UnguessableToken::Create(),
      blink::mojom::NavigationTypeForNavigationApi::kPush,
      base::Milliseconds(650)));
  EXPECT_TRUE(
      tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(soft_navs)));

  // Page comes back to foreground at 700ms.
  tracker.OnShown(base::Milliseconds(700));

  // Soft nav 4 commits in foreground at 800ms.
  soft_navs.clear();
  soft_navs.push_back(CreateSoftNavigationCommit(
      4, base::Milliseconds(800), base_time + base::Milliseconds(800),
      base::UnguessableToken::Create(),
      blink::mojom::NavigationTypeForNavigationApi::kPush,
      base::Milliseconds(850)));
  EXPECT_TRUE(
      tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(soft_navs)));

  tracker.CompleteActiveNavigationAndFlush();

  ASSERT_EQ(observer.completed_navs.size(), 3u);

  // Soft nav 2: Started in foreground, backgrounded at 500ms.
  EXPECT_EQ(observer.completed_navs[0].navigation_id, 2u);
  EXPECT_EQ(observer.completed_navs[0].first_background_time,
            base::Milliseconds(500));

  // Soft nav 3: Started in background (which began at 500ms).
  EXPECT_EQ(observer.completed_navs[1].navigation_id, 3u);
  EXPECT_EQ(observer.completed_navs[1].first_background_time,
            base::Milliseconds(500));

  // Soft nav 4: Started in foreground at 800ms, never backgrounded.
  EXPECT_EQ(observer.completed_navs[2].navigation_id, 4u);
  EXPECT_EQ(observer.completed_navs[2].first_background_time, std::nullopt);
}

TEST(SoftNavigationTrackerTest, StartedInBackground) {
  base::TimeTicks base_time = base::TimeTicks::Now();
  TestObserver observer;
  SoftNavigationTracker tracker(&observer);
  tracker.OnHidden(base::TimeDelta());

  // Soft nav 2 commits while tracker is in background.
  std::vector<mojom::SoftNavigationMetricsPtr> soft_navs;
  soft_navs.push_back(CreateSoftNavigationCommit(
      2, base::Milliseconds(100), base_time + base::Milliseconds(100),
      base::UnguessableToken::Create(),
      blink::mojom::NavigationTypeForNavigationApi::kPush,
      base::Milliseconds(150)));
  EXPECT_TRUE(
      tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(soft_navs)));

  // Page comes to foreground at 200ms.
  tracker.OnShown(base::Milliseconds(200));

  // Soft nav 3 commits in foreground at 300ms.
  soft_navs.clear();
  soft_navs.push_back(CreateSoftNavigationCommit(
      3, base::Milliseconds(300), base_time + base::Milliseconds(300),
      base::UnguessableToken::Create(),
      blink::mojom::NavigationTypeForNavigationApi::kPush,
      base::Milliseconds(350)));
  EXPECT_TRUE(
      tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(soft_navs)));

  tracker.CompleteActiveNavigationAndFlush();

  ASSERT_EQ(observer.completed_navs.size(), 2u);

  // Nav 2: started in background.
  EXPECT_EQ(observer.completed_navs[0].navigation_id, 2u);
  EXPECT_EQ(observer.completed_navs[0].first_background_time,
            base::TimeDelta());

  // Nav 3: started in foreground.
  EXPECT_EQ(observer.completed_navs[1].navigation_id, 3u);
  EXPECT_EQ(observer.completed_navs[1].first_background_time, std::nullopt);
}

TEST(SoftNavigationTrackerTest, RaceCommitArrivesAfterForeground) {
  base::TimeTicks base_time = base::TimeTicks::Now();
  TestObserver observer;
  SoftNavigationTracker tracker(&observer);

  // Soft nav 2 starts at 100ms in foreground. Page is backgrounded at 500ms and
  // foregrounded at 700ms before nav 2 commit IPC arrives.
  tracker.OnHidden(base::Milliseconds(500));
  tracker.OnShown(base::Milliseconds(700));

  // Nav 2 commit arrives late at 800ms with start_time = 100ms.
  std::vector<mojom::SoftNavigationMetricsPtr> soft_navs;
  soft_navs.push_back(CreateSoftNavigationCommit(
      2, base::Milliseconds(100), base_time + base::Milliseconds(100),
      base::UnguessableToken::Create(),
      blink::mojom::NavigationTypeForNavigationApi::kPush,
      base::Milliseconds(150)));
  EXPECT_TRUE(
      tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(soft_navs)));

  tracker.CompleteActiveNavigationAndFlush();

  ASSERT_EQ(observer.completed_navs.size(), 1u);
  EXPECT_EQ(observer.completed_navs[0].navigation_id, 2u);
  // Nav 2 was initiated in foreground (100ms < 500ms) but the page was
  // backgrounded at 500ms before nav 2 finished.
  EXPECT_EQ(observer.completed_navs[0].first_background_time,
            base::Milliseconds(500));
}

TEST(SoftNavigationTrackerTest, EventsBundledWithCommitProcessedInOrder) {
  base::TimeTicks base_time = base::TimeTicks::Now();
  TestObserver observer;
  SoftNavigationTracker tracker(&observer);

  // In a single UpdateMainFrameMetrics call, send a Commit for nav 2 along with
  // event timings, layout shifts, and soft LCP for nav 2.
  std::vector<mojom::SoftNavigationMetricsPtr> soft_navs;
  soft_navs.push_back(CreateSoftNavigationCommit(
      2, base::Milliseconds(100), base_time + base::Milliseconds(100)));

  std::vector<mojom::EventTimingPtr> events;
  events.push_back(mojom::EventTiming::New());
  events.back()->performance_timeline_navigation_id = 2;
  events.back()->duration = base::Milliseconds(35);
  events.back()->interaction_id = 1;

  std::vector<mojom::LayoutShiftPtr> shifts;
  shifts.push_back(mojom::LayoutShift::New());
  shifts.back()->performance_timeline_navigation_id = 2;
  shifts.back()->layout_shift_time = base_time + base::Milliseconds(120);
  shifts.back()->layout_shift_score = 0.15;

  std::vector<mojom::LargestContentfulPaintTimingPtr> lcps;
  lcps.push_back(mojom::LargestContentfulPaintTiming::New());
  lcps.back()->performance_timeline_navigation_id = 2;
  lcps.back()->largest_text_paint = base::Milliseconds(130);
  lcps.back()->largest_text_paint_size = 300;
  lcps.back()->type = 0;
  lcps.back()->resource_load_timings = mojom::LcpResourceLoadTimings::New();

  EXPECT_TRUE(tracker.UpdateMainFrameMetrics(kMainFrameToken, soft_navs, events,
                                             shifts, lcps));

  // Verify commit was processed first, creating the bucket and capturing all
  // bundled performance entries.
  EXPECT_EQ(tracker.soft_navigation_count(), 1u);

  const auto* nav2 = tracker.GetSoftNavigationDataForTest(2);
  ASSERT_TRUE(nav2);
  EXPECT_EQ(nav2->inp_calculator.num_user_interactions(), 1u);
  EXPECT_EQ(nav2->inp_calculator.worst_latency()->max_event.duration,
            base::Milliseconds(35));
  EXPECT_FLOAT_EQ(nav2->cls_calculator.normalized_cls_data()
                      .session_windows_gap1000ms_max5000ms_max_cls,
                  0.15f);
  EXPECT_EQ(nav2->lcp_handler.MergeMainFrameAndSubframes().Size(), 300u);
}

TEST(SoftNavigationTrackerTest, EventsBundledWithFcpAttributedBeforeFlush) {
  base::TimeTicks base_time = base::TimeTicks::Now();
  TestObserver observer;
  SoftNavigationTracker tracker(&observer);

  // Step 1: Nav 1 (id 2) commits with FCP.
  std::vector<mojom::SoftNavigationMetricsPtr> nav1;
  nav1.push_back(CreateSoftNavigationCommit(
      2, base::Milliseconds(100), base_time + base::Milliseconds(100),
      /*token=*/base::UnguessableToken::Create(),
      /*nav_type=*/blink::mojom::NavigationTypeForNavigationApi::kPush,
      /*fcp=*/base::Milliseconds(120)));
  EXPECT_TRUE(tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(nav1)));

  // Step 2: Nav 2 (id 3) commits without FCP.
  std::vector<mojom::SoftNavigationMetricsPtr> nav2;
  nav2.push_back(CreateSoftNavigationCommit(
      3, base::Milliseconds(200), base_time + base::Milliseconds(200)));
  EXPECT_TRUE(tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(nav2)));
  EXPECT_TRUE(observer.completed_nav_ids.empty());

  // Step 3: An IPC arrives containing:
  // - A late-arriving event for Nav 1 (id 2)
  // - A standalone FCP update for Nav 2 (id 3)
  std::vector<mojom::SoftNavigationMetricsPtr> fcp_nav2;
  fcp_nav2.push_back(CreateSoftNavigationFcpUpdate(3, base::Milliseconds(220)));

  std::vector<mojom::EventTimingPtr> late_nav1_events;
  late_nav1_events.push_back(mojom::EventTiming::New());
  late_nav1_events.back()->performance_timeline_navigation_id = 2;
  late_nav1_events.back()->duration = base::Milliseconds(45);
  late_nav1_events.back()->interaction_id = 9;

  EXPECT_TRUE(tracker.UpdateMainFrameMetrics(
      kMainFrameToken, std::move(fcp_nav2), late_nav1_events));

  // Nav 1 should have received the late event BEFORE being flushed on Nav 2's
  // FCP.
  ASSERT_EQ(observer.completed_navs.size(), 1u);
  EXPECT_EQ(observer.completed_navs[0].navigation_id, 2u);
  EXPECT_EQ(observer.completed_navs[0].inp_calculator.num_user_interactions(),
            1u);
  EXPECT_EQ(observer.completed_navs[0]
                .inp_calculator.worst_latency()
                ->max_event.duration,
            base::Milliseconds(45));
}

TEST(SoftNavigationTrackerTest, SubFrameMetricsAttributedByTimestampSlicing) {
  base::TimeTicks base_time = base::TimeTicks::Now();
  TestObserver observer;
  SoftNavigationTracker tracker(&observer);

  // Subframe events arriving before any soft navigation (e.g. hard nav window)
  // are not attributed to any soft navigation.
  std::vector<mojom::EventTimingPtr> pre_soft_nav_events;
  pre_soft_nav_events.push_back(mojom::EventTiming::New());
  pre_soft_nav_events.back()->start_time = base_time + base::Milliseconds(50);
  pre_soft_nav_events.back()->processing_start =
      base_time + base::Milliseconds(55);
  pre_soft_nav_events.back()->duration = base::Milliseconds(10);
  pre_soft_nav_events.back()->interaction_id = 1;

  std::vector<mojom::LayoutShiftPtr> pre_soft_nav_shifts;
  pre_soft_nav_shifts.push_back(mojom::LayoutShift::New());
  pre_soft_nav_shifts.back()->layout_shift_time =
      base_time + base::Milliseconds(60);
  pre_soft_nav_shifts.back()->layout_shift_score = 0.05;

  tracker.UpdateSubFrameMetrics(kSubFrameToken, pre_soft_nav_events,
                                pre_soft_nav_shifts);
  EXPECT_EQ(tracker.GetSoftNavigationDataForTest(2), nullptr);

  // Step 1: Soft Nav 1 commits with slicing time = base_time + 100ms.
  std::vector<mojom::SoftNavigationMetricsPtr> soft_navs_1;
  soft_navs_1.push_back(CreateSoftNavigationCommit(
      2, base::Milliseconds(100), base_time + base::Milliseconds(100)));
  EXPECT_TRUE(
      tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(soft_navs_1)));

  // Subframe interaction and layout shift during Soft Nav 1 (time = 150ms).
  std::vector<mojom::EventTimingPtr> nav1_subframe_events;
  nav1_subframe_events.push_back(mojom::EventTiming::New());
  nav1_subframe_events.back()->start_time = base_time + base::Milliseconds(150);
  nav1_subframe_events.back()->processing_start =
      base_time + base::Milliseconds(155);
  nav1_subframe_events.back()->duration = base::Milliseconds(25);
  nav1_subframe_events.back()->interaction_id = 2;

  std::vector<mojom::LayoutShiftPtr> nav1_subframe_shifts;
  nav1_subframe_shifts.push_back(mojom::LayoutShift::New());
  nav1_subframe_shifts.back()->layout_shift_time =
      base_time + base::Milliseconds(160);
  nav1_subframe_shifts.back()->layout_shift_score = 0.1;

  tracker.UpdateSubFrameMetrics(kSubFrameToken, nav1_subframe_events,
                                nav1_subframe_shifts);

  const auto* nav2 = tracker.GetSoftNavigationDataForTest(2);
  ASSERT_TRUE(nav2);
  EXPECT_EQ(nav2->inp_calculator.num_user_interactions(), 1u);
  EXPECT_EQ(nav2->inp_calculator.worst_latency()->max_event.duration,
            base::Milliseconds(25));
  EXPECT_FLOAT_EQ(nav2->cls_calculator.normalized_cls_data()
                      .session_windows_gap1000ms_max5000ms_max_cls,
                  0.1f);

  // Step 2: Soft Nav 2 commits with slicing time = base_time + 200ms.
  std::vector<mojom::SoftNavigationMetricsPtr> soft_navs_2;
  soft_navs_2.push_back(CreateSoftNavigationCommit(
      3, base::Milliseconds(200), base_time + base::Milliseconds(200)));
  EXPECT_TRUE(
      tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(soft_navs_2)));

  // Late-arriving subframe event from Soft Nav 1 (timestamp = 180ms < 200ms)
  // should attribute to Soft Nav 1 while in the queue.
  std::vector<mojom::EventTimingPtr> late_nav1_events;
  late_nav1_events.push_back(mojom::EventTiming::New());
  late_nav1_events.back()->start_time = base_time + base::Milliseconds(180);
  late_nav1_events.back()->processing_start =
      base_time + base::Milliseconds(185);
  late_nav1_events.back()->duration = base::Milliseconds(40);
  late_nav1_events.back()->interaction_id = 3;

  tracker.UpdateSubFrameMetrics(kSubFrameToken, late_nav1_events, {});

  const auto* nav2_in_queue = tracker.GetSoftNavigationDataForTest(2);
  ASSERT_TRUE(nav2_in_queue);
  EXPECT_EQ(nav2_in_queue->inp_calculator.num_user_interactions(), 2u);
  EXPECT_EQ(nav2_in_queue->inp_calculator.worst_latency()->max_event.duration,
            base::Milliseconds(40));

  // Subframe event during Soft Nav 2 (timestamp = 250ms >= 200ms)
  // should attribute to Soft Nav 2.
  std::vector<mojom::EventTimingPtr> nav2_subframe_events;
  nav2_subframe_events.push_back(mojom::EventTiming::New());
  nav2_subframe_events.back()->start_time = base_time + base::Milliseconds(250);
  nav2_subframe_events.back()->processing_start =
      base_time + base::Milliseconds(255);
  nav2_subframe_events.back()->duration = base::Milliseconds(50);
  nav2_subframe_events.back()->interaction_id = 4;

  std::vector<mojom::LayoutShiftPtr> nav2_subframe_shifts;
  nav2_subframe_shifts.push_back(mojom::LayoutShift::New());
  nav2_subframe_shifts.back()->layout_shift_time =
      base_time + base::Milliseconds(260);
  nav2_subframe_shifts.back()->layout_shift_score = 0.2;

  tracker.UpdateSubFrameMetrics(kSubFrameToken, nav2_subframe_events,
                                nav2_subframe_shifts);

  const auto* nav3 = tracker.GetSoftNavigationDataForTest(3);
  ASSERT_TRUE(nav3);
  EXPECT_EQ(nav3->inp_calculator.num_user_interactions(), 1u);
  EXPECT_EQ(nav3->inp_calculator.worst_latency()->max_event.duration,
            base::Milliseconds(50));
  EXPECT_FLOAT_EQ(nav3->cls_calculator.normalized_cls_data()
                      .session_windows_gap1000ms_max5000ms_max_cls,
                  0.2f);

  // FCP arrives for Soft Nav 1 and Soft Nav 2 -> Soft Nav 1 completes!
  std::vector<mojom::SoftNavigationMetricsPtr> fcps;
  fcps.push_back(CreateSoftNavigationFcpUpdate(2, base::Milliseconds(120)));
  fcps.push_back(CreateSoftNavigationFcpUpdate(3, base::Milliseconds(220)));
  EXPECT_TRUE(tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(fcps)));

  ASSERT_EQ(observer.completed_navs.size(), 1u);
  EXPECT_EQ(observer.completed_navs[0].navigation_id, 2u);
  EXPECT_EQ(observer.completed_navs[0].inp_calculator.num_user_interactions(),
            2u);
  EXPECT_EQ(observer.completed_navs[0]
                .inp_calculator.worst_latency()
                ->max_event.duration,
            base::Milliseconds(40));
  EXPECT_FLOAT_EQ(observer.completed_navs[0]
                      .cls_calculator.normalized_cls_data()
                      .session_windows_gap1000ms_max5000ms_max_cls,
                  0.1f);

  // Subframe events arriving after Soft Nav 1 was completed and erased are
  // ignored.
  std::vector<mojom::EventTimingPtr> post_completion_events;
  post_completion_events.push_back(mojom::EventTiming::New());
  post_completion_events.back()->start_time =
      base_time + base::Milliseconds(130);
  post_completion_events.back()->processing_start =
      base_time + base::Milliseconds(135);
  post_completion_events.back()->duration = base::Milliseconds(99);
  post_completion_events.back()->interaction_id = 5;

  tracker.UpdateSubFrameMetrics(kSubFrameToken, post_completion_events, {});
  EXPECT_EQ(tracker.GetSoftNavigationDataForTest(2), nullptr);

  // Flush completes remaining Soft Nav 2.
  tracker.CompleteActiveNavigationAndFlush();
  ASSERT_EQ(observer.completed_navs.size(), 2u);
  EXPECT_EQ(observer.completed_navs[1].navigation_id, 3u);
  EXPECT_EQ(observer.completed_navs[1].inp_calculator.num_user_interactions(),
            1u);
  EXPECT_EQ(observer.completed_navs[1]
                .inp_calculator.worst_latency()
                ->max_event.duration,
            base::Milliseconds(50));
  EXPECT_FLOAT_EQ(observer.completed_navs[1]
                      .cls_calculator.normalized_cls_data()
                      .session_windows_gap1000ms_max5000ms_max_cls,
                  0.2f);
}

TEST(SoftNavigationTrackerTest, AllInOneMultiGenerationIpcBatch) {
  base::TimeTicks base_time = base::TimeTicks::Now();
  TestObserver observer;
  SoftNavigationTracker tracker(&observer);

  // Step 1: Nav 1 (id 2) commits with FCP.
  std::vector<mojom::SoftNavigationMetricsPtr> nav1;
  nav1.push_back(CreateSoftNavigationCommit(
      2, base::Milliseconds(100), base_time + base::Milliseconds(100),
      /*token=*/base::UnguessableToken::Create(),
      /*nav_type=*/blink::mojom::NavigationTypeForNavigationApi::kPush,
      /*fcp=*/base::Milliseconds(120)));
  EXPECT_TRUE(tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(nav1)));

  // Step 2: Nav 2 (id 3) commits without FCP.
  std::vector<mojom::SoftNavigationMetricsPtr> nav2;
  nav2.push_back(CreateSoftNavigationCommit(
      3, base::Milliseconds(200), base_time + base::Milliseconds(200)));
  EXPECT_TRUE(tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(nav2)));
  EXPECT_TRUE(observer.completed_nav_ids.empty());

  // Step 3: A single IPC arrives containing:
  // - Commit for Nav 3 (id 4)
  // - Standalone FCP for Nav 2 (id 3)
  // - Late event for Nav 1 (id 2)
  // - Event for Nav 2 (id 3)
  // - Event for new Nav 3 (id 4)
  std::vector<mojom::SoftNavigationMetricsPtr> batch_soft_navs;
  batch_soft_navs.push_back(CreateSoftNavigationCommit(
      4, base::Milliseconds(300), base_time + base::Milliseconds(300)));
  batch_soft_navs.push_back(
      CreateSoftNavigationFcpUpdate(3, base::Milliseconds(220)));

  std::vector<mojom::EventTimingPtr> batch_events;
  batch_events.push_back(mojom::EventTiming::New());
  batch_events.back()->performance_timeline_navigation_id = 2;
  batch_events.back()->duration = base::Milliseconds(30);
  batch_events.back()->interaction_id = 1;

  batch_events.push_back(mojom::EventTiming::New());
  batch_events.back()->performance_timeline_navigation_id = 3;
  batch_events.back()->duration = base::Milliseconds(40);
  batch_events.back()->interaction_id = 2;

  batch_events.push_back(mojom::EventTiming::New());
  batch_events.back()->performance_timeline_navigation_id = 4;
  batch_events.back()->duration = base::Milliseconds(50);
  batch_events.back()->interaction_id = 3;

  EXPECT_TRUE(tracker.UpdateMainFrameMetrics(
      kMainFrameToken, std::move(batch_soft_navs), batch_events));

  // In this single call:
  // - Nav 3 committed (id = 4).
  // - Nav 1 received its late event (30ms).
  // - Nav 2 received its event (40ms).
  // - Nav 3 received its event (50ms).
  // - Nav 2 received its FCP (220ms).
  // - Nav 1 completed because Nav 1 and Nav 2 both have FCP.
  // - Nav 2 is waiting for Nav 3's FCP, so Nav 2 is not completed yet.
  ASSERT_EQ(observer.completed_navs.size(), 1u);
  EXPECT_EQ(observer.completed_navs[0].navigation_id, 2u);
  EXPECT_EQ(observer.completed_navs[0].inp_calculator.num_user_interactions(),
            1u);
  EXPECT_EQ(observer.completed_navs[0]
                .inp_calculator.worst_latency()
                ->max_event.duration,
            base::Milliseconds(30));

  const auto* nav2_data = tracker.GetSoftNavigationDataForTest(3);
  ASSERT_TRUE(nav2_data);
  EXPECT_EQ(nav2_data->inp_calculator.num_user_interactions(), 1u);
  EXPECT_EQ(nav2_data->inp_calculator.worst_latency()->max_event.duration,
            base::Milliseconds(40));

  const auto* nav3_data = tracker.GetSoftNavigationDataForTest(4);
  ASSERT_TRUE(nav3_data);
  EXPECT_EQ(nav3_data->inp_calculator.num_user_interactions(), 1u);
  EXPECT_EQ(nav3_data->inp_calculator.worst_latency()->max_event.duration,
            base::Milliseconds(50));
}

TEST(SoftNavigationTrackerTest, OutOfOrderFcpDeliveryWithSubFrameEvents) {
  base::TimeTicks base_time = base::TimeTicks::Now();
  TestObserver observer;
  SoftNavigationTracker tracker(&observer);

  // Commits for Nav 1 (slicing 100ms), Nav 2 (slicing 200ms), Nav 3 (slicing
  // 300ms)
  std::vector<mojom::SoftNavigationMetricsPtr> nav1;
  nav1.push_back(CreateSoftNavigationCommit(
      2, base::Milliseconds(100), base_time + base::Milliseconds(100)));
  EXPECT_TRUE(tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(nav1)));

  std::vector<mojom::SoftNavigationMetricsPtr> nav2;
  nav2.push_back(CreateSoftNavigationCommit(
      3, base::Milliseconds(200), base_time + base::Milliseconds(200)));
  EXPECT_TRUE(tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(nav2)));

  std::vector<mojom::SoftNavigationMetricsPtr> nav3;
  nav3.push_back(CreateSoftNavigationCommit(
      4, base::Milliseconds(300), base_time + base::Milliseconds(300)));
  EXPECT_TRUE(tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(nav3)));

  // Subframe events during Nav 1 (150ms), Nav 2 (250ms), Nav 3 (350ms)
  std::vector<mojom::EventTimingPtr> sub_events;
  sub_events.push_back(mojom::EventTiming::New());
  sub_events.back()->processing_start = base_time + base::Milliseconds(150);
  sub_events.back()->duration = base::Milliseconds(15);
  sub_events.back()->interaction_id = 1;

  sub_events.push_back(mojom::EventTiming::New());
  sub_events.back()->processing_start = base_time + base::Milliseconds(250);
  sub_events.back()->duration = base::Milliseconds(25);
  sub_events.back()->interaction_id = 3;

  sub_events.push_back(mojom::EventTiming::New());
  sub_events.back()->processing_start = base_time + base::Milliseconds(350);
  sub_events.back()->duration = base::Milliseconds(35);
  sub_events.back()->interaction_id = 5;

  tracker.UpdateSubFrameMetrics(kSubFrameToken, sub_events,
                                /*layout_shifts=*/{});

  // FCP1 (120ms) and FCP3 (320ms) arrive, but FCP2 is delayed!
  std::vector<mojom::SoftNavigationMetricsPtr> fcp1_and_3;
  fcp1_and_3.push_back(
      CreateSoftNavigationFcpUpdate(2, base::Milliseconds(120)));
  fcp1_and_3.push_back(
      CreateSoftNavigationFcpUpdate(4, base::Milliseconds(320)));
  EXPECT_TRUE(
      tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(fcp1_and_3)));

  // Nav 1 is still waiting for Nav 2's FCP, and Nav 2 is waiting for its own
  // FCP. Nothing should be dispatched yet.
  EXPECT_TRUE(observer.completed_nav_ids.empty());

  // Late subframe events arrive for Nav 1 (180ms) and Nav 2 (280ms). Both are
  // accepted because neither navigation has been finalized/flushed yet.
  std::vector<mojom::EventTimingPtr> late_sub_events;
  late_sub_events.push_back(mojom::EventTiming::New());
  late_sub_events.back()->processing_start =
      base_time + base::Milliseconds(180);
  late_sub_events.back()->duration = base::Milliseconds(40);
  late_sub_events.back()->interaction_id = 2;

  late_sub_events.push_back(mojom::EventTiming::New());
  late_sub_events.back()->processing_start =
      base_time + base::Milliseconds(280);
  late_sub_events.back()->duration = base::Milliseconds(50);
  late_sub_events.back()->interaction_id = 4;

  tracker.UpdateSubFrameMetrics(kSubFrameToken, late_sub_events,
                                /*layout_shifts=*/{});

  // Finally, FCP2 arrives (220ms)!
  std::vector<mojom::SoftNavigationMetricsPtr> fcp2;
  fcp2.push_back(CreateSoftNavigationFcpUpdate(3, base::Milliseconds(220)));
  EXPECT_TRUE(tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(fcp2)));

  // Now both Nav 1 and Nav 2 complete in FIFO order:
  // - Nav 1 has FCP1 and next has FCP2 -> completes!
  // - Nav 2 has FCP2 and next has FCP3 -> completes!
  ASSERT_EQ(observer.completed_navs.size(), 2u);
  EXPECT_EQ(observer.completed_navs[0].navigation_id, 2u);
  EXPECT_EQ(observer.completed_navs[0].inp_calculator.num_user_interactions(),
            2u);
  EXPECT_EQ(observer.completed_navs[0]
                .inp_calculator.worst_latency()
                ->max_event.duration,
            base::Milliseconds(40));

  EXPECT_EQ(observer.completed_navs[1].navigation_id, 3u);
  EXPECT_EQ(observer.completed_navs[1].inp_calculator.num_user_interactions(),
            2u);
  EXPECT_EQ(observer.completed_navs[1]
                .inp_calculator.worst_latency()
                ->max_event.duration,
            base::Milliseconds(50));

  // Flush completes Nav 3.
  tracker.CompleteActiveNavigationAndFlush();
  ASSERT_EQ(observer.completed_navs.size(), 3u);
  EXPECT_EQ(observer.completed_navs[2].navigation_id, 4u);
  EXPECT_EQ(observer.completed_navs[2].inp_calculator.num_user_interactions(),
            1u);
  EXPECT_EQ(observer.completed_navs[2]
                .inp_calculator.worst_latency()
                ->max_event.duration,
            base::Milliseconds(35));
}

TEST(SoftNavigationTrackerTest, MultipleSubFramesInpAttribution) {
  base::TimeTicks base_time = base::TimeTicks::Now();
  TestObserver observer;
  SoftNavigationTracker tracker(&observer);

  const content::GlobalRenderFrameHostToken kSubFrameA{
      3, blink::LocalFrameToken()};
  const content::GlobalRenderFrameHostToken kSubFrameB{
      4, blink::LocalFrameToken()};

  // Nav 1 (id 2) commits with slicing time = 100ms.
  std::vector<mojom::SoftNavigationMetricsPtr> nav1;
  nav1.push_back(CreateSoftNavigationCommit(
      2, base::Milliseconds(100), base_time + base::Milliseconds(100),
      /*token=*/base::UnguessableToken::Create(),
      /*nav_type=*/blink::mojom::NavigationTypeForNavigationApi::kPush,
      /*fcp=*/base::Milliseconds(110)));
  EXPECT_TRUE(tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(nav1)));

  // Main frame interaction during Nav 1.
  std::vector<mojom::EventTimingPtr> main_events;
  main_events.push_back(mojom::EventTiming::New());
  main_events.back()->performance_timeline_navigation_id = 2;
  main_events.back()->processing_start = base_time + base::Milliseconds(120);
  main_events.back()->duration = base::Milliseconds(30);
  main_events.back()->interaction_id = 1;
  EXPECT_TRUE(tracker.UpdateMainFrameMetrics(
      kMainFrameToken, /*soft_navigation_metrics=*/{}, main_events));

  // Subframe A interaction during Nav 1 (higher duration).
  std::vector<mojom::EventTimingPtr> sub_a_events;
  sub_a_events.push_back(mojom::EventTiming::New());
  sub_a_events.back()->processing_start = base_time + base::Milliseconds(140);
  sub_a_events.back()->duration = base::Milliseconds(65);
  sub_a_events.back()->interaction_id = 2;
  tracker.UpdateSubFrameMetrics(kSubFrameA, sub_a_events,
                                /*layout_shifts=*/{});

  // Subframe B interaction during Nav 1 (moderate duration).
  std::vector<mojom::EventTimingPtr> sub_b_events;
  sub_b_events.push_back(mojom::EventTiming::New());
  sub_b_events.back()->processing_start = base_time + base::Milliseconds(160);
  sub_b_events.back()->duration = base::Milliseconds(45);
  sub_b_events.back()->interaction_id = 3;
  tracker.UpdateSubFrameMetrics(kSubFrameB, sub_b_events,
                                /*layout_shifts=*/{});

  const auto* nav2 = tracker.GetSoftNavigationDataForTest(2);
  ASSERT_TRUE(nav2);
  EXPECT_EQ(nav2->inp_calculator.num_user_interactions(), 3u);
  EXPECT_EQ(nav2->inp_calculator.worst_latency()->max_event.duration,
            base::Milliseconds(65));
}

TEST(SoftNavigationTrackerTest, MainAndSubFrameClsSessionWindows) {
  base::TimeTicks base_time = base::TimeTicks::Now();
  TestObserver observer;
  SoftNavigationTracker tracker(&observer);

  // Nav 1 (id 2) commits with slicing time = 100ms.
  std::vector<mojom::SoftNavigationMetricsPtr> nav1;
  nav1.push_back(CreateSoftNavigationCommit(
      2, base::Milliseconds(100), base_time + base::Milliseconds(100),
      /*token=*/base::UnguessableToken::Create(),
      /*nav_type=*/blink::mojom::NavigationTypeForNavigationApi::kPush,
      /*fcp=*/base::Milliseconds(110)));
  EXPECT_TRUE(tracker.UpdateMainFrameMetrics(kMainFrameToken, std::move(nav1)));

  // Main frame layout shift during Nav 1.
  std::vector<mojom::LayoutShiftPtr> main_shifts;
  main_shifts.push_back(mojom::LayoutShift::New());
  main_shifts.back()->performance_timeline_navigation_id = 2;
  main_shifts.back()->layout_shift_time = base_time + base::Milliseconds(120);
  main_shifts.back()->layout_shift_score = 0.12;
  EXPECT_TRUE(tracker.UpdateMainFrameMetrics(
      kMainFrameToken, /*soft_navigation_metrics=*/{}, {}, main_shifts));

  // Subframe layout shift during Nav 1 in same session window.
  std::vector<mojom::LayoutShiftPtr> sub_shifts;
  sub_shifts.push_back(mojom::LayoutShift::New());
  sub_shifts.back()->layout_shift_time = base_time + base::Milliseconds(150);
  sub_shifts.back()->layout_shift_score = 0.08;
  tracker.UpdateSubFrameMetrics(kSubFrameToken, {}, sub_shifts);

  const auto* nav2 = tracker.GetSoftNavigationDataForTest(2);
  ASSERT_TRUE(nav2);
  EXPECT_FLOAT_EQ(nav2->cls_calculator.normalized_cls_data()
                      .session_windows_gap1000ms_max5000ms_max_cls,
                  0.20f);
}

}  // namespace page_load_metrics
