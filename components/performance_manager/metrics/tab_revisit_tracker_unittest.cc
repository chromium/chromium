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

namespace performance_manager {

class TabRevisitTrackerTest : public GraphTestHarness {
 protected:
  void SetUp() override {
    GraphTestHarness::SetUp();

    graph()->PassToGraph(std::make_unique<TabPageDecorator>());
    graph()->PassToGraph(std::make_unique<TabRevisitTracker>());
  }

  void SetIsActiveTab(const PageNode* page_node, bool is_active) {
    PageLiveStateDecorator::Data* data =
        PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node);
    data->SetIsActiveTabForTesting(is_active);
  }
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
  tester.ExpectUniqueTimeSample(TabRevisitTracker::kTimeToRevisitHistogramName,
                                base::Minutes(30), 1);

  SetIsActiveTab(mock_graph.page.get(), false);
  tester.ExpectUniqueTimeSample(TabRevisitTracker::kTimeToRevisitHistogramName,
                                base::Minutes(30), 1);

  AdvanceClock(base::Minutes(10));
  // The tab became active again after 10 minutes in the background, the revisit
  // histogram should contain 2 samples: one for each revisit.
  SetIsActiveTab(mock_graph.page.get(), true);
  tester.ExpectTotalCount(TabRevisitTracker::kTimeToRevisitHistogramName, 2);
  tester.ExpectTimeBucketCount(TabRevisitTracker::kTimeToRevisitHistogramName,
                               base::Minutes(10), 1);

  tester.ExpectTotalCount(TabRevisitTracker::kTimeToCloseHistogramName, 0);
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
  tester.ExpectUniqueTimeSample(TabRevisitTracker::kTimeToCloseHistogramName,
                                base::Hours(1), 1);
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
}

}  // namespace performance_manager
