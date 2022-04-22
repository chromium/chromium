// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/query_clusters_state.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/history_clusters_service_test_api.h"
#include "components/history_clusters/core/history_clusters_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace history_clusters {

namespace {

// A struct containing all the params in `QueryClustersState::ResultCallback`.
struct OnGotClustersResult {
  std::string query;
  std::vector<history::Cluster> cluster_batch;
  bool can_load_more;
  bool is_continuation;
};

}  // namespace

class QueryClustersStateTest : public testing::Test {
 public:
  QueryClustersStateTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME) {}

  QueryClustersStateTest(const QueryClustersStateTest&) = delete;
  QueryClustersStateTest& operator=(const QueryClustersStateTest&) = delete;

 protected:
  OnGotClustersResult InjectRawClustersAndAwaitPostProcessing(
      QueryClustersState* state,
      const std::vector<history::Cluster>& raw_clusters,
      QueryClustersContinuationParams continuation_params) {
    // This block injects the fake `raw_clusters` data for post-processing and
    // spins the message loop until we finish post-processing.
    OnGotClustersResult result;
    base::RunLoop loop;
    state->OnGotRawClusters(
        base::TimeTicks(),
        base::BindLambdaForTesting(
            [&](const std::string& query,
                std::vector<history::Cluster> cluster_batch, bool can_load_more,
                bool is_continuation) {
              result = {query, cluster_batch, can_load_more, is_continuation};
              loop.Quit();
            }),
        raw_clusters, continuation_params);
    loop.Run();
    return result;
  }

  void InjectRawClustersAndExpectNoCallback(
      QueryClustersState* state,
      const std::vector<history::Cluster>& raw_clusters,
      QueryClustersContinuationParams continuation_params) {
    state->OnGotRawClusters(
        base::TimeTicks(),
        base::BindLambdaForTesting(
            [&](const std::string& query,
                std::vector<history::Cluster> cluster_batch, bool can_load_more,
                bool is_continuation) {
              FAIL() << "Callback should not have been called.";
            }),
        raw_clusters, continuation_params);
  }

  base::test::TaskEnvironment task_environment_;
};

TEST_F(QueryClustersStateTest, PostProcessingOccursAndLogsHistograms) {
  base::HistogramTester histogram_tester;
  QueryClustersState state(nullptr, "");

  std::vector<history::Cluster> raw_clusters;
  raw_clusters.push_back(
      history::Cluster(1, {}, {u"keyword_one"},
                       /*should_show_on_prominent_ui_surfaces=*/false));
  raw_clusters.push_back(
      history::Cluster(2, {}, {u"keyword_two"},
                       /*should_show_on_prominent_ui_surfaces=*/true));

  auto result =
      InjectRawClustersAndAwaitPostProcessing(&state, raw_clusters, {});

  // Just a basic test to verify that post-processing did indeed occur.
  // Detailed tests for the behavior of the filtering are in
  // `HistoryClustersUtil`.
  ASSERT_EQ(result.cluster_batch.size(), 1U);
  EXPECT_EQ(result.cluster_batch[0].cluster_id, 2);

  EXPECT_EQ(result.query, "");
  EXPECT_EQ(result.can_load_more, true);
  EXPECT_EQ(result.is_continuation, false);

  histogram_tester.ExpectBucketCount(
      "History.Clusters.PercentClustersFilteredByQuery", 50, 1);
  histogram_tester.ExpectTotalCount("History.Clusters.ServiceLatency", 1);
}

TEST_F(QueryClustersStateTest, CrossBatchDeduplication) {
  QueryClustersState state(nullptr, "myquery");

  {
    std::vector<history::Cluster> raw_clusters;
    // Verify that non-matching prominent clusters are filtered out.
    raw_clusters.push_back(
        history::Cluster(1, {}, {u"keyword_one"},
                         /*should_show_on_prominent_ui_surfaces=*/true));
    // Verify that matching non-prominent clusters still are shown.
    raw_clusters.push_back(
        history::Cluster(2, {GetHardcodedClusterVisit(1)}, {u"myquery"},
                         /*should_show_on_prominent_ui_surfaces=*/false));

    auto result =
        InjectRawClustersAndAwaitPostProcessing(&state, raw_clusters, {});

    ASSERT_EQ(result.cluster_batch.size(), 1U);
    EXPECT_EQ(result.cluster_batch[0].cluster_id, 2);
    ASSERT_EQ(result.cluster_batch[0].visits.size(), 1U);
    EXPECT_EQ(
        result.cluster_batch[0].visits[0].annotated_visit.visit_row.visit_id,
        1);

    EXPECT_EQ(result.query, "myquery");
    EXPECT_EQ(result.can_load_more, true);
    EXPECT_EQ(result.is_continuation, false);
  }

  // Send through a second batch of raw clusters. This verifies the stateful
  // cross-batch de-duplication.
  {
    std::vector<history::Cluster> raw_clusters;
    // Verify that a matching non-prominent non-duplicate cluster is still
    // allowed.
    raw_clusters.push_back(
        history::Cluster(3, {GetHardcodedClusterVisit(2)}, {u"myquery"},
                         /*should_show_on_prominent_ui_surfaces=*/false));

    // Verify that a matching non-prominent duplicate cluster is filtered out.
    raw_clusters.push_back(
        history::Cluster(4, {GetHardcodedClusterVisit(1)}, {u"myquery"},
                         /*should_show_on_prominent_ui_surfaces=*/false));

    // Verify that a matching prominent duplicate cluster is still allowed.
    raw_clusters.push_back(
        history::Cluster(5, {GetHardcodedClusterVisit(1)}, {u"myquery"},
                         /*should_show_on_prominent_ui_surfaces=*/true));

    auto result =
        InjectRawClustersAndAwaitPostProcessing(&state, raw_clusters, {});

    ASSERT_EQ(result.cluster_batch.size(), 2U);
    EXPECT_EQ(result.cluster_batch[0].cluster_id, 3);
    EXPECT_EQ(result.cluster_batch[1].cluster_id, 5);

    EXPECT_EQ(result.query, "myquery");
    EXPECT_EQ(result.can_load_more, true);
    EXPECT_EQ(result.is_continuation, true);
  }
}

TEST_F(QueryClustersStateTest, OnGotClusters) {
  const history::Cluster hidden_cluster = {1, {}, {}, false};
  const history::Cluster visible_cluster = {2, {}, {}, true};

  {
    QueryClustersState state(nullptr, "");

    // If the response clusters is empty, the callback should not be invoked.
    InjectRawClustersAndExpectNoCallback(&state, {},
                                         {base::Time(),
                                          /*is_continuation=*/false,
                                          /*is_partial_day=*/false,
                                          /*exhausted_history=*/false,
                                          /*is_done=*/false});

    // Likewise if the response clusters is filtered.
    InjectRawClustersAndExpectNoCallback(&state, {hidden_cluster},
                                         {base::Time(),
                                          /*is_continuation=*/false,
                                          /*is_partial_day=*/false,
                                          /*exhausted_history=*/false,
                                          /*is_done=*/false});

    // Even if the clusters is empty, the callback should be called when history
    // is exhausted.
    // Also, `is_continuation` should be false since this is the first time the
    // callbacks invoked even though there's been 2 earlier
    // `HistoryClusterService` responses.
    auto result =
        InjectRawClustersAndAwaitPostProcessing(&state, {},
                                                {base::Time(),
                                                 /*is_continuation=*/false,
                                                 /*is_partial_day=*/false,
                                                 /*exhausted_history=*/true,
                                                 /*is_done=*/true});
    EXPECT_EQ(result.cluster_batch.size(), 0u);
    EXPECT_EQ(result.query, "");
    EXPECT_EQ(result.can_load_more, false);
    EXPECT_EQ(result.is_continuation, false);
  }

  {
    QueryClustersState state(nullptr, "");

    // `is_continuation` should be false on the first callback.
    // `can_load_more` should be true on non-last callback.
    auto result =
        InjectRawClustersAndAwaitPostProcessing(&state, {visible_cluster},
                                                {base::Time(),
                                                 /*is_continuation=*/false,
                                                 /*is_partial_day=*/false,
                                                 /*exhausted_history=*/false,
                                                 /*is_done=*/false});
    EXPECT_EQ(result.cluster_batch.size(), 1u);
    EXPECT_EQ(result.query, "");
    EXPECT_EQ(result.can_load_more, true);
    EXPECT_EQ(result.is_continuation, false);

    // `is_continuation` should be true on non-first callback.
    // `can_load_more` should be true on non-last callback.
    result =
        InjectRawClustersAndAwaitPostProcessing(&state, {visible_cluster},
                                                {base::Time(),
                                                 /*is_continuation=*/true,
                                                 /*is_partial_day=*/false,
                                                 /*exhausted_history=*/false,
                                                 /*is_done=*/false});
    EXPECT_EQ(result.cluster_batch.size(), 1u);
    EXPECT_EQ(result.query, "");
    EXPECT_EQ(result.can_load_more, true);
    EXPECT_EQ(result.is_continuation, true);

    // `can_load_more` should be false on the last callback.
    result =
        InjectRawClustersAndAwaitPostProcessing(&state, {visible_cluster},
                                                {base::Time(),
                                                 /*is_continuation=*/true,
                                                 /*is_partial_day=*/false,
                                                 /*exhausted_history=*/true,
                                                 /*is_done=*/true});
    EXPECT_EQ(result.cluster_batch.size(), 1u);
    EXPECT_EQ(result.query, "");
    EXPECT_EQ(result.can_load_more, false);
    EXPECT_EQ(result.is_continuation, true);
  }
}

}  // namespace history_clusters
