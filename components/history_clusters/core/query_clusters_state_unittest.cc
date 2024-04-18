// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/query_clusters_state.h"

#include "base/feature_list.h"
#include "base/run_loop.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/core/history_clusters_service_test_api.h"
#include "components/history_clusters/core/history_clusters_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::Invoke;
using ::testing::SaveArg;

namespace history_clusters {

namespace {

// A struct containing all the params in `QueryClustersState::ResultCallback`.
struct OnGotClustersResult {
  std::string query;
  std::vector<history::Cluster> cluster_batch;
  bool can_load_more;
  bool is_continuation;
};

class MockHistoryService : public history::HistoryService {
 public:
  MockHistoryService() = default;
  ~MockHistoryService() override = default;

  MOCK_METHOD(base::CancelableTaskTracker::TaskId,
              GetAnnotatedVisits,
              (const history::QueryOptions& options,
               bool compute_redirect_chain_start_properties,
               bool get_unclustered_visits_only,
               GetAnnotatedVisitsCallback callback,
               base::CancelableTaskTracker* tracker),
              (const override));
};

}  // namespace

class QueryClustersStateTest : public testing::Test {
 public:
  QueryClustersStateTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME) {}

  QueryClustersStateTest(const QueryClustersStateTest&) = delete;
  QueryClustersStateTest& operator=(const QueryClustersStateTest&) = delete;

  QueryClustersFilterParams GetQueryClustersFilterParamsForState(
      QueryClustersState* state) {
    return state->filter_params_;
  }

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

TEST_F(QueryClustersStateTest, FilterParamsSetForZeroState) {
  Config config;
  config.apply_zero_state_filtering = true;
  config.use_navigation_context_clusters = true;
  SetConfigForTesting(config);

  QueryClustersState state(nullptr, nullptr, "");

  QueryClustersFilterParams filter_params =
      GetQueryClustersFilterParamsForState(&state);
  EXPECT_TRUE(filter_params.is_search_initiated);
  EXPECT_FALSE(filter_params.has_related_searches);
}

TEST_F(QueryClustersStateTest, FilterParamsNotSetForZeroStateFeatureDisabled) {
  Config config;
  config.apply_zero_state_filtering = false;
  config.use_navigation_context_clusters = true;
  SetConfigForTesting(config);

  QueryClustersState state(nullptr, nullptr, "");

  QueryClustersFilterParams filter_params =
      GetQueryClustersFilterParamsForState(&state);
  EXPECT_FALSE(filter_params.is_search_initiated);
  EXPECT_FALSE(filter_params.has_related_searches);
}

TEST_F(QueryClustersStateTest,
       FilterParamsNotSetForZeroStateContextClusteringDisabled) {
  Config config;
  config.apply_zero_state_filtering = true;
  config.use_navigation_context_clusters = false;
  SetConfigForTesting(config);

  QueryClustersState state(nullptr, nullptr, "");

  QueryClustersFilterParams filter_params =
      GetQueryClustersFilterParamsForState(&state);
  EXPECT_FALSE(filter_params.is_search_initiated);
  EXPECT_FALSE(filter_params.has_related_searches);
}

TEST_F(QueryClustersStateTest, FilterParamsEnabledButNotSetForQuery) {
  Config config;
  config.apply_zero_state_filtering = true;
  config.use_navigation_context_clusters = true;
  SetConfigForTesting(config);

  QueryClustersState state(nullptr, nullptr, "query");

  QueryClustersFilterParams filter_params =
      GetQueryClustersFilterParamsForState(&state);
  EXPECT_FALSE(filter_params.is_search_initiated);
  EXPECT_FALSE(filter_params.has_related_searches);
}

TEST_F(QueryClustersStateTest, PostProcessingOccursAndLogsHistograms) {
  base::HistogramTester histogram_tester;
  QueryClustersState state(nullptr, nullptr, "");

  std::vector<history::Cluster> raw_clusters;
  raw_clusters.push_back(history::Cluster(
      1, {GetHardcodedClusterVisit(1), GetHardcodedClusterVisit(2)},
      {{u"keyword_one", history::ClusterKeywordData()}},
      /*should_show_on_prominent_ui_surfaces=*/false));
  raw_clusters.push_back(history::Cluster(
      2, {GetHardcodedClusterVisit(3), GetHardcodedClusterVisit(4)},
      {{u"keyword_two", history::ClusterKeywordData()}},
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
  QueryClustersState state(nullptr, nullptr, "myquery");

  {
    std::vector<history::Cluster> raw_clusters;
    // Verify that non-matching prominent clusters are filtered out.
    raw_clusters.push_back(history::Cluster(
        1, {}, {{u"keyword_one", history::ClusterKeywordData()}},
        /*should_show_on_prominent_ui_surfaces=*/true));
    // Verify that matching non-prominent clusters still are shown.
    raw_clusters.push_back(
        history::Cluster(2, {GetHardcodedClusterVisit(1)},
                         {{u"myquery", history::ClusterKeywordData()}},
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
        history::Cluster(3, {GetHardcodedClusterVisit(2)},
                         {{u"myquery", history::ClusterKeywordData()}},
                         /*should_show_on_prominent_ui_surfaces=*/false));

    // Verify that a matching non-prominent duplicate cluster is filtered out.
    raw_clusters.push_back(
        history::Cluster(4, {GetHardcodedClusterVisit(1)},
                         {{u"myquery", history::ClusterKeywordData()}},
                         /*should_show_on_prominent_ui_surfaces=*/false));

    // Verify that a matching prominent duplicate cluster is still allowed.
    raw_clusters.push_back(
        history::Cluster(5, {GetHardcodedClusterVisit(1)},
                         {{u"myquery", history::ClusterKeywordData()}},
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
  const history::Cluster hidden_cluster = {
      1, {GetHardcodedClusterVisit(1), GetHardcodedClusterVisit(2)}, {}, false};
  const history::Cluster visible_cluster = {
      2, {GetHardcodedClusterVisit(3), GetHardcodedClusterVisit(4)}, {}, true};

  {
    QueryClustersState state(nullptr, nullptr, "");

    // If the response clusters is empty, the callback should not be invoked.
    InjectRawClustersAndExpectNoCallback(
        &state, {},
        {base::Time(),
         /*is_continuation=*/false,
         /*is_partial_day=*/false,
         /*exhausted_unclustered_visits=*/false,
         /*exhausted_all_visits=*/false});

    // Likewise if the response clusters is filtered.
    InjectRawClustersAndExpectNoCallback(
        &state, {hidden_cluster},
        {base::Time(),
         /*is_continuation=*/false,
         /*is_partial_day=*/false,
         /*exhausted_unclustered_visits=*/false,
         /*exhausted_all_visits=*/false});

    // Even if the clusters is empty, the callback should be called when history
    // is exhausted.
    // Also, `is_continuation` should be false since this is the first time the
    // callbacks invoked even though there's been 2 earlier
    // `HistoryClusterService` responses.
    auto result = InjectRawClustersAndAwaitPostProcessing(
        &state, {},
        {base::Time(),
         /*is_continuation=*/false,
         /*is_partial_day=*/false,
         /*exhausted_unclustered_visits=*/true,
         /*exhausted_all_visits=*/true});
    EXPECT_EQ(result.cluster_batch.size(), 0u);
    EXPECT_EQ(result.query, "");
    EXPECT_EQ(result.can_load_more, false);
    EXPECT_EQ(result.is_continuation, false);
  }

  {
    QueryClustersState state(nullptr, nullptr, "");

    // `is_continuation` should be false on the first callback.
    // `can_load_more` should be true on non-last callback.
    auto result = InjectRawClustersAndAwaitPostProcessing(
        &state, {visible_cluster},
        {base::Time(),
         /*is_continuation=*/false,
         /*is_partial_day=*/false,
         /*exhausted_unclustered_visits=*/false,
         /*exhausted_all_visits=*/false});
    EXPECT_EQ(result.cluster_batch.size(), 1u);
    EXPECT_EQ(result.query, "");
    EXPECT_EQ(result.can_load_more, true);
    EXPECT_EQ(result.is_continuation, false);

    // `is_continuation` should be true on non-first callback.
    // `can_load_more` should be true on non-last callback.
    result = InjectRawClustersAndAwaitPostProcessing(
        &state, {visible_cluster},
        {base::Time(),
         /*is_continuation=*/true,
         /*is_partial_day=*/false,
         /*exhausted_unclustered_visits=*/false,
         /*exhausted_all_visits=*/false});
    EXPECT_EQ(result.cluster_batch.size(), 1u);
    EXPECT_EQ(result.query, "");
    EXPECT_EQ(result.can_load_more, true);
    EXPECT_EQ(result.is_continuation, true);

    // `can_load_more` should be false on the last callback.
    result = InjectRawClustersAndAwaitPostProcessing(
        &state, {visible_cluster},
        {base::Time(),
         /*is_continuation=*/true,
         /*is_partial_day=*/false,
         /*exhausted_unclustered_visits=*/true,
         /*exhausted_all_visits=*/true});
    EXPECT_EQ(result.cluster_batch.size(), 1u);
    EXPECT_EQ(result.query, "");
    EXPECT_EQ(result.can_load_more, false);
    EXPECT_EQ(result.is_continuation, true);
  }
}

TEST_F(QueryClustersStateTest, UniqueRawLabels) {
  QueryClustersState state(nullptr, nullptr, "");

  std::vector<history::ClusterVisit> cluster_visits = {
      GetHardcodedClusterVisit(1), GetHardcodedClusterVisit(2)};

  auto cluster1 = history::Cluster(1, cluster_visits, {});
  cluster1.raw_label = u"rawlabel1";
  auto cluster2 = history::Cluster(2, cluster_visits, {});
  cluster2.raw_label = u"rawlabel2";
  auto cluster3 = history::Cluster(3, cluster_visits, {});
  cluster3.raw_label = u"rawlabel3";

  // Now make some clusters with repeated raw labels.
  auto cluster4 = history::Cluster(4, cluster_visits, {});
  cluster4.raw_label = u"rawlabel1";
  auto cluster5 = history::Cluster(5, cluster_visits, {});
  cluster5.raw_label = u"rawlabel2";

  auto result = InjectRawClustersAndAwaitPostProcessing(
      &state, {cluster1, cluster2, cluster4}, {});
  ASSERT_EQ(result.cluster_batch.size(), 3U);
  EXPECT_THAT(state.raw_label_counts_so_far(),
              ElementsAre(std::make_pair(u"rawlabel1", 2),
                          std::make_pair(u"rawlabel2", 1)));

  // Test updating an existing count, and adding new ones after that.
  result =
      InjectRawClustersAndAwaitPostProcessing(&state, {cluster5, cluster3}, {});
  ASSERT_EQ(result.cluster_batch.size(), 2U);
  EXPECT_THAT(state.raw_label_counts_so_far(),
              ElementsAre(std::make_pair(u"rawlabel1", 2),
                          std::make_pair(u"rawlabel2", 2),
                          std::make_pair(u"rawlabel3", 1)));
}

TEST_F(QueryClustersStateTest, GetUngroupedVisits) {
  base::test::ScopedFeatureList scoped_list(kSearchesFindUngroupedVisits);

  MockHistoryService mock_history_service;

  history::QueryOptions get_annotated_visits_options;
  base::RunLoop get_ungrouped_visits_loop_1;
  base::RunLoop get_ungrouped_visits_loop_2;
  EXPECT_CALL(mock_history_service, GetAnnotatedVisits)
      .Times(2)
      .WillOnce([&](const history::QueryOptions& options,
                    bool compute_redirect_chain_start_properties,
                    bool get_unclustered_visits_only,
                    MockHistoryService::GetAnnotatedVisitsCallback callback,
                    base::CancelableTaskTracker* tracker) {
        get_annotated_visits_options = options;
        get_ungrouped_visits_loop_1.Quit();
        return base::CancelableTaskTracker::kBadTaskId;
      })
      .WillOnce([&](const history::QueryOptions& options,
                    bool compute_redirect_chain_start_properties,
                    bool get_unclustered_visits_only,
                    MockHistoryService::GetAnnotatedVisitsCallback callback,
                    base::CancelableTaskTracker* tracker) {
        get_annotated_visits_options = options;
        get_ungrouped_visits_loop_2.Quit();
        return base::CancelableTaskTracker::kBadTaskId;
      });

  QueryClustersState state(nullptr, /*history_service=*/&mock_history_service,
                           /*query=*/"Code");

  base::RunLoop result_loop;
  std::vector<history::Cluster> final_result;
  auto result_callback = [&](const std::string& query,
                             std::vector<history::Cluster> cluster_batch,
                             bool can_load_more, bool is_continuation) {
    final_result = std::move(cluster_batch);
    result_loop.Quit();
  };

  QueryClustersContinuationParams fake_continuation_params;
  fake_continuation_params.is_continuation = false;
  base::Time continuation_time;
  ASSERT_TRUE(
      base::Time::FromUTCString("14 Feb 2021 10:00", &continuation_time));
  fake_continuation_params.continuation_time = continuation_time;

  // Verify that `QueryClustersState` makes an initial call to the
  // HistoryService that makes sense.
  {
    state.GetUngroupedVisits(base::TimeTicks(),
                             base::BindLambdaForTesting(result_callback), {},
                             fake_continuation_params);
    // Will quit the loop once GetAnnotatedVisits is run.
    get_ungrouped_visits_loop_1.Run();

    EXPECT_EQ(get_annotated_visits_options.begin_time, continuation_time);
    EXPECT_EQ(get_annotated_visits_options.end_time, base::Time());
  }

  // Verify that the ungrouped visits can be searched over and returned as part
  // of a special ungrouped cluster.
  {
    state.OnGotUngroupedVisits(base::TimeTicks(),
                               base::BindLambdaForTesting(result_callback), {},
                               fake_continuation_params,
                               /*ungrouped_visits*/ GetHardcodedTestVisits());
    result_loop.Run();
    ASSERT_EQ(final_result.size(), 1U);
    EXPECT_EQ(final_result[0].label_source,
              history::Cluster::LabelSource::kUngroupedVisits);
    EXPECT_EQ(final_result[0].visits[0].annotated_visit.url_row.id(), 2);
  }

  // Verify that the followup search for ungrouped visits spans the correct
  // continuation times.
  {
    QueryClustersContinuationParams new_fake_continuation_params;
    new_fake_continuation_params.is_continuation = true;
    base::Time new_continuation_time;
    ASSERT_TRUE(
        base::Time::FromUTCString("12 Feb 2021 10:00", &new_continuation_time));
    new_fake_continuation_params.continuation_time = new_continuation_time;

    state.GetUngroupedVisits(base::TimeTicks(),
                             base::BindLambdaForTesting(result_callback), {},
                             new_fake_continuation_params);
    // Will quit the loop once GetAnnotatedVisits is run.
    get_ungrouped_visits_loop_2.Run();

    EXPECT_EQ(get_annotated_visits_options.begin_time, new_continuation_time);
    EXPECT_EQ(get_annotated_visits_options.end_time, continuation_time);
  }
}

TEST_F(QueryClustersStateTest, GetUngroupedVisitsDoesCrossBatchDeduplication) {
  base::test::ScopedFeatureList scoped_list(kSearchesFindUngroupedVisits);

  MockHistoryService mock_history_service;
  QueryClustersState state(nullptr, /*history_service=*/&mock_history_service,
                           /*query=*/"Code");

  // First batch of ungrouped visits seeds the seen visits set with url_id = 2.
  {
    base::RunLoop result_loop;
    std::vector<history::Cluster> final_result;

    state.OnGotUngroupedVisits(
        base::TimeTicks(),
        base::BindLambdaForTesting(
            [&](const std::string& query,
                std::vector<history::Cluster> cluster_batch, bool can_load_more,
                bool is_continuation) {
              final_result = std::move(cluster_batch);
              result_loop.Quit();
            }),
        {}, QueryClustersContinuationParams(),
        /*ungrouped_visits=*/GetHardcodedTestVisits());
    result_loop.Run();
    ASSERT_EQ(final_result.size(), 1U);
    EXPECT_EQ(final_result[0].label_source,
              history::Cluster::LabelSource::kUngroupedVisits);
    EXPECT_EQ(final_result[0].visits[0].annotated_visit.url_row.id(), 2);
  }

  // Second batch of ungrouped visits gets deduplicated.
  {
    base::RunLoop result_loop;
    std::vector<history::Cluster> final_result;

    // Prevent the code from trying to fetch ANOTHER batch.
    QueryClustersContinuationParams continuation_params;
    continuation_params.exhausted_all_visits = true;

    state.OnGotUngroupedVisits(
        base::TimeTicks(),
        base::BindLambdaForTesting(
            [&](const std::string& query,
                std::vector<history::Cluster> cluster_batch, bool can_load_more,
                bool is_continuation) {
              final_result = std::move(cluster_batch);
              result_loop.Quit();
            }),
        {}, continuation_params,
        /*ungrouped_visits=*/GetHardcodedTestVisits());
    result_loop.Run();
    EXPECT_TRUE(final_result.empty());
  }
}

}  // namespace history_clusters
