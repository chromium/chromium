// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/metrics/tab_revisit_tracker.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace performance_manager {

// Hard-coding these constants instead of using `static_cast` on the
// `TabRevisitTracker::State` enum to guard against changes to the enum that
// would make it out of sync with the enums.xml entry.
constexpr int64_t kActiveState = 0;
constexpr int64_t kBackgroundState = 1;
constexpr int64_t kClosedState = 2;

class TabRevisitTrackerTest : public GraphTestHarness {
 protected:
  void SetUp() override {
    GraphTestHarness::SetUp();

    graph()->PassToGraph(std::make_unique<TabPageDecorator>());
    graph()->PassToGraph(std::make_unique<TabRevisitTracker>());

    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  void SetIsActiveTab(const PageNode* page_node, bool is_active) {
    PageLiveStateDecorator::Data* data =
        PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node);
    data->SetIsActiveTabForTesting(is_active);
  }

  void ValidateEntry(size_t entries_count,
                     size_t entry_id,
                     int64_t previous_state,
                     int64_t new_state,
                     int64_t num_total_revisits,
                     base::TimeDelta time_in_previous_state) {
    auto entries = test_ukm_recorder_->GetEntriesByName(
        ukm::builders::TabRevisitTracker_TabStateChange::kEntryName);
    EXPECT_EQ(entries.size(), entries_count);
    EXPECT_GT(entries.size(), entry_id);
    test_ukm_recorder_->ExpectEntryMetric(entries[entry_id], "NewState",
                                          new_state);
    test_ukm_recorder_->ExpectEntryMetric(entries[entry_id], "PreviousState",
                                          previous_state);
    test_ukm_recorder_->ExpectEntryMetric(entries[entry_id], "NumTotalRevisits",
                                          num_total_revisits);
    test_ukm_recorder_->ExpectEntryMetric(
        entries[entry_id], "TimeInPreviousState",
        TabRevisitTracker::ExponentiallyBucketedSeconds(
            time_in_previous_state));
  }

  std::unique_ptr<ukm::TestUkmRecorder> test_ukm_recorder_;
};

TEST_F(TabRevisitTrackerTest, StartsBackgroundedThenRevisited) {
  base::HistogramTester tester;
  MockSinglePageInSingleProcessGraph mock_graph(graph());

  // Creating the graph doesn't record anything since the page nodes are created
  // as kUnknown and don't change their "active tab" status.
  tester.ExpectTotalCount(TabRevisitTracker::kTimeToRevisitHistogramName, 0);

  SetIsActiveTab(mock_graph.page.get(), false);
  mock_graph.page->SetType(PageType::kTab);
  tester.ExpectTotalCount(TabRevisitTracker::kTimeToRevisitHistogramName, 0);

  AdvanceClock(base::Minutes(30));
  tester.ExpectTotalCount(TabRevisitTracker::kTimeToRevisitHistogramName, 0);

  SetIsActiveTab(mock_graph.page.get(), true);
  // The tab became active after 30 minutes in the background, this should be
  // recorded in the revisit histogram.
  tester.ExpectUniqueSample(TabRevisitTracker::kTimeToRevisitHistogramName,
                            base::Minutes(30).InSeconds(), 1);

  ValidateEntry(/*entries_count=*/1, /*entry_id=*/0,
                /*previous_state=*/kBackgroundState,
                /*new_state=*/kActiveState,
                /*num_total_revisits=*/1,
                /*time_in_previous_state=*/base::Minutes(30));

  SetIsActiveTab(mock_graph.page.get(), false);
  tester.ExpectUniqueSample(TabRevisitTracker::kTimeToRevisitHistogramName,
                            base::Minutes(30).InSeconds(), 1);

  ValidateEntry(/*entries_count=*/2, /*entry_id=*/1,
                /*previous_state=*/kActiveState,
                /*new_state=*/kBackgroundState,
                /*num_total_revisits=*/1,
                /*time_in_previous_state=*/base::TimeDelta());

  AdvanceClock(base::Minutes(10));
  // The tab became active again after 10 minutes in the background, the revisit
  // histogram should contain 2 samples: one for each revisit.
  SetIsActiveTab(mock_graph.page.get(), true);
  tester.ExpectTotalCount(TabRevisitTracker::kTimeToRevisitHistogramName, 2);
  tester.ExpectBucketCount(TabRevisitTracker::kTimeToRevisitHistogramName,
                           base::Minutes(10).InSeconds(), 1);

  tester.ExpectTotalCount(TabRevisitTracker::kTimeToCloseHistogramName, 0);

  ValidateEntry(/*entries_count=*/3, /*entry_id=*/2,
                /*previous_state=*/kBackgroundState,
                /*new_state=*/kActiveState,
                /*num_total_revisits=*/2,
                /*time_in_previous_state=*/base::Minutes(10));
}

TEST_F(TabRevisitTrackerTest, CloseInBackgroundRecordsToCloseHistogram) {
  base::HistogramTester tester;
  MockSinglePageInSingleProcessGraph mock_graph(graph());

  SetIsActiveTab(mock_graph.page.get(), false);
  mock_graph.page->SetType(PageType::kTab);
  tester.ExpectTotalCount(TabRevisitTracker::kTimeToRevisitHistogramName, 0);
  tester.ExpectTotalCount(TabRevisitTracker::kTimeToCloseHistogramName, 0);

  AdvanceClock(base::Hours(1));

  // Closing the tab while it's inactive should record to the close histogram
  // but not the revisit one.
  mock_graph.frame.reset();
  mock_graph.page.reset();

  tester.ExpectTotalCount(TabRevisitTracker::kTimeToRevisitHistogramName, 0);

  tester.ExpectUniqueSample(TabRevisitTracker::kTimeToCloseHistogramName,
                            base::Hours(1).InSeconds(), 1);

  ValidateEntry(/*entries_count=*/1, /*entry_id=*/0,
                /*previous_state=*/kBackgroundState,
                /*new_state=*/kClosedState,
                /*num_total_revisits=*/0,
                /*time_in_previous_state=*/base::Hours(1));
}

TEST_F(TabRevisitTrackerTest, CloseWhileActiveDoesntRecordClose) {
  base::HistogramTester tester;
  MockSinglePageInSingleProcessGraph mock_graph(graph());

  SetIsActiveTab(mock_graph.page.get(), true);
  mock_graph.page->SetType(PageType::kTab);
  tester.ExpectTotalCount(TabRevisitTracker::kTimeToRevisitHistogramName, 0);
  tester.ExpectTotalCount(TabRevisitTracker::kTimeToCloseHistogramName, 0);

  AdvanceClock(base::Hours(1));

  // Closing the tab while it's active doesn't record either histogram, since
  // they are only concerned about background tabs closing or becoming active.
  mock_graph.frame.reset();
  mock_graph.page.reset();

  tester.ExpectTotalCount(TabRevisitTracker::kTimeToRevisitHistogramName, 0);
  tester.ExpectTotalCount(TabRevisitTracker::kTimeToCloseHistogramName, 0);

  ValidateEntry(/*entries_count=*/1, /*entry_id=*/0,
                /*previous_state=*/kActiveState,
                /*new_state=*/kClosedState,
                /*num_total_revisits=*/0,
                /*time_in_previous_state=*/base::Hours(1));
}

}  // namespace performance_manager
