// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/history_clusters_service.h"

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/history/core/browser/history_context.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/history/core/test/visit_annotations_test_utils.h"
#include "components/history_clusters/core/clustering_backend.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/core/history_clusters_db_tasks.h"
#include "components/history_clusters/core/history_clusters_service_task_get_most_recent_clusters.h"
#include "components/history_clusters/core/history_clusters_service_test_api.h"
#include "components/history_clusters/core/history_clusters_types.h"
#include "components/history_clusters/core/history_clusters_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace history_clusters {

namespace {

base::Time DaysAgo(int days) {
  return base::Time::Now() - base::Days(days);
}

// Trivial backend to allow us to specifically test just the service behavior.
class TestClusteringBackend : public ClusteringBackend {
 public:
  void GetClusters(ClusteringRequestSource clustering_request_source,
                   ClustersCallback callback,
                   std::vector<history::AnnotatedVisit> visits) override {
    callback_ = std::move(callback);
    last_clustered_visits_ = visits;

    std::move(wait_for_get_clusters_closure_).Run();
  }

  void FulfillCallback(const std::vector<history::Cluster>& clusters) {
    std::move(callback_).Run(clusters);
  }

  const std::vector<history::AnnotatedVisit>& LastClusteredVisits() const {
    return last_clustered_visits_;
  }

  // Fetches a scored visit by an ID. `visit_id` must be valid. This is a
  // convenience method used for constructing the fake response.
  history::ClusterVisit GetVisitById(int visit_id) {
    for (const auto& visit : last_clustered_visits_) {
      if (visit.visit_row.visit_id == visit_id)
        return AnnotatedVisitToClusterVisit(visit);
    }

    NOTREACHED()
        << "TestClusteringBackend::GetVisitById() could not find visit_id: "
        << visit_id;
    return {};
  }

  // Should be invoked before `GetClusters()` is invoked.
  void WaitForGetClustersCall() {
    base::RunLoop loop;
    wait_for_get_clusters_closure_ = loop.QuitClosure();
    loop.Run();
  }

 private:
  base::OnceClosure wait_for_get_clusters_closure_;

  ClustersCallback callback_;
  std::vector<history::AnnotatedVisit> last_clustered_visits_;
};

class HistoryClustersServiceTestBase : public testing::Test {
 public:
  HistoryClustersServiceTestBase()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME),
        run_loop_quit_(run_loop_.QuitClosure()) {}

  void SetUp() override {
    CHECK(history_dir_.CreateUniqueTempDir());
    history_service_ =
        history::CreateHistoryService(history_dir_.GetPath(), true);
    history_clusters_service_ = std::make_unique<HistoryClustersService>(
        "en-US", history_service_.get(),
        /*entity_metadata_provider=*/nullptr,
        /*url_loader_factory=*/nullptr,
        /*engagement_score_provider=*/nullptr,
        /*template_url_service=*/nullptr,
        /*optimization_guide_decider=*/nullptr);

    history_clusters_service_test_api_ =
        std::make_unique<HistoryClustersServiceTestApi>(
            history_clusters_service_.get(), history_service_.get());
    auto test_backend = std::make_unique<TestClusteringBackend>();
    test_clustering_backend_ = test_backend.get();
    history_clusters_service_test_api_->SetClusteringBackendForTest(
        std::move(test_backend));
  }

  HistoryClustersServiceTestBase(const HistoryClustersServiceTestBase&) =
      delete;
  HistoryClustersServiceTestBase& operator=(
      const HistoryClustersServiceTestBase&) = delete;

  // Add hardcoded completed visits with context annotations to the history
  // database.
  void AddHardcodedTestDataToHistoryService() {
    for (auto& visit : GetHardcodedTestVisits())
      AddCompleteVisit(visit);
  }

  // Add a complete visit with context annotations to the history database.
  void AddCompleteVisit(const history::AnnotatedVisit& visit) {
    static const history::ContextID context_id = 1;

    history::HistoryAddPageArgs add_page_args;
    add_page_args.context_id = context_id;
    add_page_args.nav_entry_id = next_navigation_id_;
    add_page_args.url = visit.url_row.url();
    add_page_args.title = visit.url_row.title();
    add_page_args.time = visit.visit_row.visit_time;
    add_page_args.visit_source = visit.source;
    history_service_->AddPage(add_page_args);
    history_service_->UpdateWithPageEndTime(
        context_id, next_navigation_id_, visit.url_row.url(),
        visit.visit_row.visit_time + visit.visit_row.visit_duration);

    auto& incomplete_visit_context_annotations =
        history_clusters_service_->GetOrCreateIncompleteVisitContextAnnotations(
            next_navigation_id_);
    incomplete_visit_context_annotations.visit_row = visit.visit_row;
    incomplete_visit_context_annotations.url_row = visit.url_row;
    incomplete_visit_context_annotations.context_annotations =
        visit.context_annotations;
    incomplete_visit_context_annotations.status.history_rows = true;
    incomplete_visit_context_annotations.status.navigation_ended = true;
    incomplete_visit_context_annotations.status.navigation_end_signals = true;
    history_clusters_service_->CompleteVisitContextAnnotationsIfReady(
        next_navigation_id_);

    next_navigation_id_++;
  }

  // Like `AddCompleteVisit()` above but with less input provided.
  void AddCompleteVisit(history::VisitID visit_id, base::Time visit_time) {
    history::AnnotatedVisit visit;
    visit.url_row.set_id(1);
    visit.visit_row.visit_id = visit_id;
    visit.visit_row.visit_time = visit_time;
    visit.source = history::VisitSource::SOURCE_BROWSED;
    AddCompleteVisit(visit);
  }

  // Add an incomplete visit context annotations to the in memory incomplete
  // visit map. Does not touch the history database.
  void AddIncompleteVisit(
      history::URLID url_id,
      history::VisitID visit_id,
      base::Time visit_time,
      ui::PageTransition transition = ui::PageTransitionFromInt(
          ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_CHAIN_START |
          ui::PAGE_TRANSITION_CHAIN_END)) {
    // It's not possible to have an incomplete visit with URL or visit set but
    // not the other. The IDs must either both be 0 or both be non-zero.
    ASSERT_FALSE(url_id ^ visit_id);
    auto& incomplete_visit_context_annotations =
        history_clusters_service_->GetOrCreateIncompleteVisitContextAnnotations(
            next_navigation_id_);
    incomplete_visit_context_annotations.url_row.set_id(url_id);
    incomplete_visit_context_annotations.visit_row.visit_id = visit_id;
    incomplete_visit_context_annotations.visit_row.visit_time = visit_time;
    incomplete_visit_context_annotations.visit_row.transition = transition;
    incomplete_visit_context_annotations.status.history_rows = url_id;
    next_navigation_id_++;
  }

  void AddCluster(std::vector<history::VisitID> visit_ids) {
    base::CancelableTaskTracker task_tracker;
    history_service_->ReplaceClusters({}, {history::CreateCluster(visit_ids)},
                                      base::DoNothing(), &task_tracker);
    history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());
  }

  // Verifies that the hardcoded visits were passed to the clustering backend.
  void AwaitAndVerifyTestClusteringBackendRequest(bool expect_synced_visits) {
    test_clustering_backend_->WaitForGetClustersCall();

    std::vector<history::AnnotatedVisit> visits =
        test_clustering_backend_->LastClusteredVisits();

    // Visits 2, 3, and 5 are 1-day-old; visit 3 is a synced visit.
    ASSERT_EQ(visits.size(), expect_synced_visits ? 3u : 2u);

    auto& visit = visits[0];
    EXPECT_EQ(visit.visit_row.visit_id, 5);
    EXPECT_EQ(visit.visit_row.visit_time,
              GetHardcodedTestVisits()[4].visit_row.visit_time);
    EXPECT_EQ(visit.visit_row.visit_duration, base::Seconds(20));
    EXPECT_EQ(visit.url_row.url(), "https://second-1-day-old-visit.com/");
    EXPECT_EQ(visit.context_annotations.page_end_reason, 5);

    visit = visits[1];
    if (expect_synced_visits) {
      EXPECT_EQ(visit.visit_row.visit_id, 3);
      EXPECT_EQ(visit.visit_row.visit_time,
                GetHardcodedTestVisits()[2].visit_row.visit_time);
      EXPECT_EQ(visit.visit_row.visit_duration, base::Seconds(20));
      EXPECT_EQ(visit.url_row.url(), "https://synched-visit.com/");
      EXPECT_EQ(visit.context_annotations.page_end_reason, 5);

      visit = visits[2];
    }
    EXPECT_EQ(visit.visit_row.visit_id, 2);
    EXPECT_EQ(visit.visit_row.visit_time,
              GetHardcodedTestVisits()[1].visit_row.visit_time);
    EXPECT_EQ(visit.visit_row.visit_duration, base::Seconds(20));
    EXPECT_EQ(visit.url_row.url(), "https://github.com/");
    EXPECT_EQ(visit.context_annotations.page_end_reason, 5);

    // TODO(tommycli): Add back visit.referring_visit_id() check after updating
    //  the HistoryService test methods to support that field.
  }

  // Helper to repeatedly call `QueryClusters` and return the clusters it
  // returns as well as the visits that were sent to `ClusteringBackend`. Will
  // verify a request to the clustering backend is or is NOT made depending on
  // `expect_clustering_backend_call`.
  std::pair<std::vector<history::Cluster>, std::vector<history::AnnotatedVisit>>
  NextQueryClusters(QueryClustersContinuationParams& continuation_params,
                    bool expect_clustering_backend_call = true) {
    std::vector<history::Cluster> clusters;
    base::RunLoop loop;
    const auto task = history_clusters_service_->QueryClusters(
        ClusteringRequestSource::kJourneysPage,
        /*begin_time=*/base::Time(), continuation_params, /*recluster=*/false,
        base::BindLambdaForTesting(
            [&](std::vector<history::Cluster> clusters_temp,
                QueryClustersContinuationParams continuation_params_temp) {
              loop.Quit();
              clusters = clusters_temp;
              continuation_params = continuation_params_temp;
            }),
        HistoryClustersServiceTaskGetMostRecentClusters::Source::kWebUi);

    // If we expect a clustering call, expect a request and return no clusters.
    if (expect_clustering_backend_call) {
      test_clustering_backend_->WaitForGetClustersCall();
      test_clustering_backend_->FulfillCallback({});
    }

    // Wait for all the async stuff to complete.
    loop.Run();

    // Give history a chance to flush out the task to avoid memory leaks.
    history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());

    // Persisted visits are ordered before incomplete visits. Persisted visits
    // are ordered newest first. Incomplete visits are ordered the same as they
    // were sent to the `HistoryClustersService`.
    return {clusters, expect_clustering_backend_call
                          ? test_clustering_backend_->LastClusteredVisits()
                          : std::vector<history::AnnotatedVisit>{}};
  }

  // Helper to repeatedly schedule a `GetAnnotatedVisitsToCluster` and return
  // the clusters and visits it returns.
  std::pair<std::vector<int64_t>, std::vector<history::AnnotatedVisit>>
  NextVisits(QueryClustersContinuationParams& continuation_params,
             bool recent_first,
             int days_of_clustered_visits) {
    std::vector<int64_t> old_clusters;
    std::vector<history::AnnotatedVisit> visits;
    base::CancelableTaskTracker task_tracker;
    history_service_->ScheduleDBTask(
        FROM_HERE,
        std::make_unique<GetAnnotatedVisitsToCluster>(
            IncompleteVisitMap{}, base::Time(), continuation_params,
            recent_first, days_of_clustered_visits, /*recluster=*/false,
            base::BindLambdaForTesting(
                [&](std::vector<int64_t> old_clusters_temp,
                    std::vector<history::AnnotatedVisit> visits_temp,
                    QueryClustersContinuationParams continuation_params_temp) {
                  old_clusters = old_clusters_temp;
                  visits = visits_temp;
                  continuation_params = continuation_params_temp;
                })),
        &task_tracker);
    history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());
    return {old_clusters, visits};
  }

  // Helper to flush out the multiple history and cluster backend requests made
  // by `Does[Query|URL]MatchAnyCluster()`. It won't populate the cache until
  // all its requests have been completed. It makes 1 request (to each) per
  // unique day with at least 1 visit; i.e. `number_of_days_with_visits`.
  void FlushKeywordRequests(std::vector<history::Cluster> clusters,
                            size_t number_of_days_with_visits) {
    // `Does[Query|URL]MatchAnyCluster()` will continue making history and
    // cluster backend requests until it has exhausted history. We have to flush
    // out these requests before it will populate the cache.
    for (size_t i = 0; i < number_of_days_with_visits; ++i) {
      test_clustering_backend_->WaitForGetClustersCall();
      test_clustering_backend_->FulfillCallback(
          i == 0 ? clusters : std::vector<history::Cluster>{});
    }
    // Flush out the last, empty history requests. There'll be 2 history
    // requests: the 1st to exhaust visits to cluster requests, and the 2nd to
    // exhaust persisted cluster requests.
    history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());
    history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());
  }

 protected:
  // ScopedFeatureList needs to be declared before TaskEnvironment, so that it
  // is destroyed after the TaskEnvironment is destroyed, preventing other
  // threads from accessing the feature list while it's being destroyed.
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;

  // Used to construct a `HistoryClustersService`.
  base::ScopedTempDir history_dir_;
  std::unique_ptr<history::HistoryService> history_service_;

  std::unique_ptr<HistoryClustersService> history_clusters_service_;
  std::unique_ptr<HistoryClustersServiceTestApi>
      history_clusters_service_test_api_;

  // Non-owning pointer. The actual owner is `history_clusters_service_`.
  TestClusteringBackend* test_clustering_backend_;

  // Used to verify the async callback is invoked.
  base::RunLoop run_loop_;
  base::RepeatingClosure run_loop_quit_;

  // Tracks the next available navigation ID to be associated with visits.
  int64_t next_navigation_id_ = 0;
};

class HistoryClustersServiceTest : public HistoryClustersServiceTestBase,
                                   public ::testing::WithParamInterface<bool> {
 public:
  HistoryClustersServiceTest() {
    scoped_feature_list_.InitAndEnableFeature(internal::kJourneys);
    Config config;
    config.persist_clusters_in_history_db = true;
    config.include_synced_visits = ExpectSyncedVisits();
    SetConfigForTesting(config);
  }

  // Whether synced visits are expected to be sent to the clustering backend.
  bool ExpectSyncedVisits() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(IncludeSyncedVisits,
                         HistoryClustersServiceTest,
                         ::testing::Bool());

TEST_P(HistoryClustersServiceTest, HardCapOnVisitsFetchedFromHistory) {
  Config config;
  config.is_journeys_enabled_no_locale_check = true;
  config.max_visits_to_cluster = 20;
  SetConfigForTesting(config);

  history::ContextID context_id = 1;
  auto visit = GetHardcodedTestVisits()[0];
  for (size_t i = 0; i < 100; ++i) {
    // Visit IDs start at 1.
    visit.visit_row.visit_id = i + 1;

    history::HistoryAddPageArgs add_page_args;
    add_page_args.context_id = context_id;
    add_page_args.nav_entry_id = next_navigation_id_;
    add_page_args.url = visit.url_row.url();
    add_page_args.title = visit.url_row.title();
    add_page_args.time = visit.visit_row.visit_time;
    add_page_args.visit_source = visit.source;
    history_service_->AddPage(add_page_args);
    history_service_->UpdateWithPageEndTime(
        context_id, next_navigation_id_, visit.url_row.url(),
        visit.visit_row.visit_time + visit.visit_row.visit_duration);

    auto& incomplete_visit_context_annotations =
        history_clusters_service_->GetOrCreateIncompleteVisitContextAnnotations(
            next_navigation_id_);
    incomplete_visit_context_annotations.visit_row = visit.visit_row;
    incomplete_visit_context_annotations.url_row = visit.url_row;
    incomplete_visit_context_annotations.context_annotations =
        visit.context_annotations;
    incomplete_visit_context_annotations.status.history_rows = true;
    incomplete_visit_context_annotations.status.navigation_ended = true;
    incomplete_visit_context_annotations.status.navigation_end_signals = true;
    history_clusters_service_->CompleteVisitContextAnnotationsIfReady(
        next_navigation_id_);
    next_navigation_id_++;
  }
  history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());

  const auto task = history_clusters_service_->QueryClusters(
      ClusteringRequestSource::kKeywordCacheGeneration,
      /*begin_time=*/base::Time(), /*continuation_params=*/{},
      /*recluster=*/false,
      base::DoNothing(),  // Only need to verify the correct request is sent
      HistoryClustersServiceTaskGetMostRecentClusters::Source::kWebUi);

  test_clustering_backend_->WaitForGetClustersCall();
  history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());

  EXPECT_EQ(test_clustering_backend_->LastClusteredVisits().size(), 20U);
}

TEST_P(HistoryClustersServiceTest, QueryClusters_IncompleteAndPersistedVisits) {
  // Create 5 persisted visits with visit times 2, 1, 1, 60, and 1 days ago.
  AddHardcodedTestDataToHistoryService();

  // Create incomplete visits; only 6 & 7 should be returned by the query.
  AddIncompleteVisit(6, 6, DaysAgo(1));
  AddIncompleteVisit(0, 0, DaysAgo(1));  // Missing history rows.
  AddIncompleteVisit(7, 7, DaysAgo(90));
  AddIncompleteVisit(8, 8, DaysAgo(0));   // Too recent.
  AddIncompleteVisit(9, 9, DaysAgo(93));  // Too old.
  AddIncompleteVisit(
      10, 10, DaysAgo(1),
      ui::PageTransitionFromInt(805306372));  // Non-visible page transition.

  QueryClustersContinuationParams continuation_params = {};
  continuation_params.continuation_time = base::Time::Now();

  // 1st query should return visits 2, 3, 5, & 6, the good, 1-day-old visits.
  // Visit 0 is excluded because it's missing history rows. Visit 10 is excluded
  // because it has a non-visible transition.
  {
    const auto [clusters, visits] = NextQueryClusters(continuation_params);
    EXPECT_THAT(GetClusterIds(clusters), testing::ElementsAre());
    if (ExpectSyncedVisits()) {
      EXPECT_THAT(GetVisitIds(visits), testing::ElementsAre(5, 3, 2, 6));
    } else {
      EXPECT_THAT(GetVisitIds(visits), testing::ElementsAre(5, 2, 6));
    }
    EXPECT_TRUE(continuation_params.is_continuation);
    EXPECT_FALSE(continuation_params.is_partial_day);
  }
  // 2nd query should return visit 1, a 2-day-old complete visit.
  {
    const auto [clusters, visits] = NextQueryClusters(continuation_params);
    EXPECT_THAT(GetClusterIds(clusters), testing::ElementsAre());
    EXPECT_THAT(GetVisitIds(visits), testing::ElementsAre(1));
  }
  // 3rd query should return visit 4, a 30-day-old complete visit, since there
  // are no 3-to-29-day-old visits.
  {
    const auto [clusters, visits] = NextQueryClusters(continuation_params);
    EXPECT_THAT(GetClusterIds(clusters), testing::ElementsAre());
    EXPECT_THAT(GetVisitIds(visits), testing::ElementsAre(4));
    EXPECT_FALSE(continuation_params.exhausted_unclustered_visits);
    EXPECT_FALSE(continuation_params.exhausted_all_visits);
  }
  // 4th query should return visit 7, a 90-day-old incomplete visit, since there
  // are no 31-to-89-day-old visits.
  {
    const auto [clusters, visits] = NextQueryClusters(continuation_params);
    EXPECT_THAT(GetClusterIds(clusters), testing::ElementsAre());
    EXPECT_THAT(GetVisitIds(visits), testing::ElementsAre(7));
    EXPECT_TRUE(continuation_params.exhausted_unclustered_visits);
    EXPECT_TRUE(continuation_params.exhausted_all_visits);
  }
}

TEST_P(HistoryClustersServiceTest,
       QueryClusters_PersistedClusters_NoMixedDays) {
  // Test the case where there are persisted clusters but none on a day also
  // containing unclustered visits.

  // 2 unclustered visits.
  AddCompleteVisit(1, DaysAgo(1));
  AddCompleteVisit(2, DaysAgo(2));

  // 2 clustered visits; i.e. persisted clusters.
  AddCompleteVisit(3, DaysAgo(3));
  AddCompleteVisit(4, DaysAgo(4));
  AddCluster({3});
  AddCluster({4});

  // Another clustered visit with a gap.
  AddCompleteVisit(5, DaysAgo(10));
  AddCluster({5});

  // The DB looks like:
  // Days ago: 10 9 8 7 6 5 4 3 2 1
  // Visit:    C            C C U U
  // Where C & U are clustered & unclustered visits.

  QueryClustersContinuationParams continuation_params = {};
  continuation_params.continuation_time = base::Time::Now();

  // 1st 2 queries should return the 2 unclustered visits.
  {
    const auto [clusters, visits] = NextQueryClusters(continuation_params);
    EXPECT_THAT(GetClusterIds(clusters), testing::ElementsAre());
    EXPECT_THAT(GetVisitIds(visits), testing::ElementsAre(1));
    EXPECT_FALSE(continuation_params.exhausted_unclustered_visits);
    EXPECT_FALSE(continuation_params.exhausted_all_visits);
  }
  {
    const auto [clusters, visits] = NextQueryClusters(continuation_params);
    EXPECT_THAT(GetClusterIds(clusters), testing::ElementsAre());
    EXPECT_THAT(GetVisitIds(visits), testing::ElementsAre(2));
    EXPECT_FALSE(continuation_params.exhausted_unclustered_visits);
    EXPECT_FALSE(continuation_params.exhausted_all_visits);
  }
  // Next query should return all 3 persisted clusters. it should not make a
  // request to the clustering backend. And it should set
  // `exhausted_unclustered_visits`.
  {
    const auto [clusters, visits] =
        NextQueryClusters(continuation_params, false);
    ASSERT_THAT(GetClusterIds(clusters), testing::ElementsAre(1, 2, 3));
    EXPECT_THAT(GetVisitIds(clusters[0].visits), testing::ElementsAre(3));
    EXPECT_THAT(GetVisitIds(clusters[1].visits), testing::ElementsAre(4));
    EXPECT_THAT(GetVisitIds(clusters[2].visits), testing::ElementsAre(5));
    EXPECT_THAT(GetVisitIds(visits), testing::ElementsAre());
    EXPECT_TRUE(continuation_params.exhausted_unclustered_visits);
    EXPECT_FALSE(continuation_params.exhausted_all_visits);
  }
  // The last query should set `exhausted_all_visits`.
  {
    const auto [clusters, visits] =
        NextQueryClusters(continuation_params, false);
    EXPECT_THAT(GetClusterIds(clusters), testing::ElementsAre());
    EXPECT_THAT(GetVisitIds(visits), testing::ElementsAre());
    EXPECT_TRUE(continuation_params.exhausted_unclustered_visits);
    EXPECT_TRUE(continuation_params.exhausted_all_visits);
  }
}

TEST_P(HistoryClustersServiceTest,
       QueryClusters_PersistedClusters_PersistenceDisabled) {
  // Test the case where there are persisted clusters but persistence is
  // disabled to check users who were in an enabled then disabled group
  // don't encounter weirdness.

  Config config;
  config.persist_clusters_in_history_db = false;
  SetConfigForTesting(config);

  // Unclustered visit.
  AddCompleteVisit(1, DaysAgo(1));

  // Clustered visit; i.e. persisted cluster.
  AddCompleteVisit(2, DaysAgo(2));
  AddCluster({2});

  QueryClustersContinuationParams continuation_params = {};
  continuation_params.continuation_time = base::Time::Now();

  // 2 queries should return the 2 visits and treat both as unclustered.
  {
    const auto [clusters, visits] = NextQueryClusters(continuation_params);
    EXPECT_THAT(GetClusterIds(clusters), testing::ElementsAre());
    EXPECT_THAT(GetVisitIds(visits), testing::ElementsAre(1));
    EXPECT_FALSE(continuation_params.exhausted_unclustered_visits);
    EXPECT_FALSE(continuation_params.exhausted_all_visits);
  }
  {
    const auto [clusters, visits] = NextQueryClusters(continuation_params);
    EXPECT_THAT(GetClusterIds(clusters), testing::ElementsAre());
    EXPECT_THAT(GetVisitIds(visits), testing::ElementsAre(2));
    EXPECT_FALSE(continuation_params.exhausted_unclustered_visits);
    EXPECT_FALSE(continuation_params.exhausted_all_visits);
  }
  // 3rd query should consider history exhausted.
  {
    const auto [clusters, visits] =
        NextQueryClusters(continuation_params, false);
    EXPECT_THAT(GetClusterIds(clusters), testing::ElementsAre());
    EXPECT_THAT(GetVisitIds(visits), testing::ElementsAre());
    EXPECT_TRUE(continuation_params.exhausted_unclustered_visits);
    EXPECT_TRUE(continuation_params.exhausted_all_visits);
  }
}

TEST_P(HistoryClustersServiceTest, QueryClusters_PersistedClusters_Today) {
  // Test the case where there is a persisted cluster today. The task rewinds
  // the query bounds when it reaches a clustered visit, and this should be done
  // correctly even if it's at the edge.

  // Can't use `Now()`, as the task only searches [now-90, now).
  const auto today = base::Time::Now() - base::Hours(1);

  // A clustered and unclustered visit, both today.
  AddCompleteVisit(1, today);
  AddCompleteVisit(2, today);
  AddCluster({2});

  QueryClustersContinuationParams continuation_params = {};
  continuation_params.continuation_time = base::Time::Now();

  // 1st query should return the 1st unclustered visits only  and set
  // `exhausted_unclustered_visits`.
  {
    const auto [clusters, visits] = NextQueryClusters(continuation_params);
    EXPECT_THAT(GetClusterIds(clusters), testing::ElementsAre());
    EXPECT_THAT(GetVisitIds(visits), testing::ElementsAre(1));
    EXPECT_TRUE(continuation_params.exhausted_unclustered_visits);
    EXPECT_FALSE(continuation_params.exhausted_all_visits);
  }
  // 2nd query should return the cluster.
  {
    const auto [clusters, visits] =
        NextQueryClusters(continuation_params, false);
    ASSERT_THAT(GetClusterIds(clusters), testing::ElementsAre(1));
    EXPECT_THAT(GetVisitIds(clusters[0].visits), testing::ElementsAre(2));
    EXPECT_THAT(GetVisitIds(visits), testing::ElementsAre());
    EXPECT_TRUE(continuation_params.exhausted_unclustered_visits);
    EXPECT_FALSE(continuation_params.exhausted_all_visits);
  }
  // The last query should set `exhausted_all_visits`.
  {
    const auto [clusters, visits] =
        NextQueryClusters(continuation_params, false);
    EXPECT_THAT(GetClusterIds(clusters), testing::ElementsAre());
    EXPECT_THAT(GetVisitIds(visits), testing::ElementsAre());
    EXPECT_TRUE(continuation_params.exhausted_unclustered_visits);
    EXPECT_TRUE(continuation_params.exhausted_all_visits);
  }
}

TEST_P(HistoryClustersServiceTest, QueryClusters_PersistedClusters_MixedDay) {
  // Test the case where there are persisted clusters on a day also containing
  // unclustered visits.

  // 2 unclustered visits.
  AddCompleteVisit(1, DaysAgo(1));
  AddCompleteVisit(2, DaysAgo(2));

  // 2 clustered visits; i.e. persisted clusters.
  AddCompleteVisit(3, DaysAgo(2));
  AddCompleteVisit(4, DaysAgo(3));
  AddCluster({3});
  AddCluster({4});

  // The DB looks like:
  // Days ago: 3 2 1
  // Visit:    C M U
  // Where C, U, & M are days containing clustered, unclustered, and mixed
  // visits.

  QueryClustersContinuationParams continuation_params = {};
  continuation_params.continuation_time = base::Time::Now();

  // 1st query should return the unclustered visit.
  {
    const auto [clusters, visits] = NextQueryClusters(continuation_params);
    EXPECT_THAT(GetClusterIds(clusters), testing::ElementsAre());
    EXPECT_THAT(GetVisitIds(visits), testing::ElementsAre(1));
    EXPECT_FALSE(continuation_params.exhausted_unclustered_visits);
    EXPECT_FALSE(continuation_params.exhausted_all_visits);
  }
  // 2nd query should return only the unclustered visit. Should also set
  // `exhausted_unclustered_visits`.
  {
    const auto [clusters, visits] = NextQueryClusters(continuation_params);
    EXPECT_THAT(GetClusterIds(clusters), testing::ElementsAre());
    EXPECT_THAT(GetVisitIds(visits), testing::ElementsAre(2));
    EXPECT_TRUE(continuation_params.exhausted_unclustered_visits);
    EXPECT_FALSE(continuation_params.exhausted_all_visits);
  }
  // 3rd query should return the 1st cluster from 2 days ago; it shouldn't be
  // skipped even though the 2nd query already returned a visit from 2 days ago.
  // It should also return the non-mixed cluster.
  {
    const auto [clusters, visits] =
        NextQueryClusters(continuation_params, false);
    ASSERT_THAT(GetClusterIds(clusters), testing::ElementsAre(1, 2));
    EXPECT_THAT(GetVisitIds(clusters[0].visits), testing::ElementsAre(3));
    EXPECT_THAT(GetVisitIds(clusters[1].visits), testing::ElementsAre(4));
    EXPECT_THAT(GetVisitIds(visits), testing::ElementsAre());
    EXPECT_TRUE(continuation_params.exhausted_unclustered_visits);
    EXPECT_FALSE(continuation_params.exhausted_all_visits);
  }
  // Last query should set `exhausted_all_visits`.
  {
    const auto [clusters, visits] =
        NextQueryClusters(continuation_params, false);
    EXPECT_THAT(GetClusterIds(clusters), testing::ElementsAre());
    EXPECT_THAT(GetVisitIds(visits), testing::ElementsAre());
    EXPECT_TRUE(continuation_params.exhausted_unclustered_visits);
    EXPECT_TRUE(continuation_params.exhausted_all_visits);
  }
}

TEST_P(HistoryClustersServiceTest, QueryVisits_OldestFirst) {
  // Create 5 persisted visits with visit times 2, 1, 1, 60, and 1 days ago.
  AddHardcodedTestDataToHistoryService();

  // Helper to repeatedly schedule a `GetAnnotatedVisitsToCluster`, with the
  // continuation time returned from the previous task, and return the visits
  // it returns.
  QueryClustersContinuationParams continuation_params = {};

  {
    // 1st query should return the oldest, 60-day-old visit.
    const auto [clusters, visits] = NextVisits(continuation_params, false, 0);
    EXPECT_TRUE(clusters.empty());
    EXPECT_THAT(GetVisitIds(visits), testing::ElementsAre(4));
    EXPECT_TRUE(continuation_params.is_continuation);
    EXPECT_FALSE(continuation_params.is_partial_day);
    EXPECT_FALSE(continuation_params.exhausted_unclustered_visits);
    EXPECT_FALSE(continuation_params.exhausted_all_visits);
  }
  {
    // 2nd query should return the next oldest, 2-day-old visit.
    const auto [clusters, visits] = NextVisits(continuation_params, false, 0);
    EXPECT_TRUE(clusters.empty());
    EXPECT_THAT(GetVisitIds(visits), testing::ElementsAre(1));
    EXPECT_TRUE(continuation_params.is_continuation);
    EXPECT_FALSE(continuation_params.exhausted_unclustered_visits);
    EXPECT_FALSE(continuation_params.exhausted_all_visits);
  }
  {
    // 3rd query should return the next oldest, 1-day-old visits. Visit 3 is
    // is from sync, and is still included.
    const auto [clusters, visits] = NextVisits(continuation_params, false, 0);
    EXPECT_TRUE(clusters.empty());
    if (ExpectSyncedVisits()) {
      EXPECT_THAT(GetVisitIds(visits), testing::ElementsAre(5, 3, 2));
    } else {
      EXPECT_THAT(GetVisitIds(visits), testing::ElementsAre(5, 2));
    }
    EXPECT_TRUE(continuation_params.is_continuation);
    EXPECT_FALSE(continuation_params.exhausted_unclustered_visits);
    EXPECT_FALSE(continuation_params.exhausted_all_visits);
  }
  {
    // 4th query should return no visits; all visits were exhausted.
    const auto [clusters, visits] = NextVisits(continuation_params, false, 0);
    EXPECT_TRUE(clusters.empty());
    EXPECT_TRUE(visits.empty());
    EXPECT_TRUE(continuation_params.exhausted_unclustered_visits);
    EXPECT_TRUE(continuation_params.exhausted_all_visits);
  }
}

TEST_P(HistoryClustersServiceTest, QueryClusteredVisits) {
  // Create unclustered visits 1, 2, 3, and 4 days-old.
  AddCompleteVisit(1, DaysAgo(1));
  AddCompleteVisit(2, DaysAgo(2));
  AddCompleteVisit(3, DaysAgo(3));
  AddCompleteVisit(4, DaysAgo(4));

  // Create clustered visits 3 and 4 days-old.
  AddCompleteVisit(5, DaysAgo(3));
  AddCompleteVisit(6, DaysAgo(4));
  AddCluster({5});
  AddCluster({6});

  QueryClustersContinuationParams continuation_params = {};

  {
    // 1st query should get the newest, 1-day-old, visit. There are no adjacent
    // clusters to get.
    const auto [clusters, visits] = NextVisits(continuation_params, true, 1);
    EXPECT_TRUE(clusters.empty());
    EXPECT_THAT(GetVisitIds(visits), testing::ElementsAre(1));
    EXPECT_TRUE(continuation_params.is_continuation);
    EXPECT_FALSE(continuation_params.is_partial_day);
    EXPECT_FALSE(continuation_params.exhausted_unclustered_visits);
    EXPECT_FALSE(continuation_params.exhausted_all_visits);
  }
  {
    // 2nd query should get the 2-day-old visit and the adjacent
    // 3-day-old clustered visit. Should not get the 3-day-old or older
    // unclustered visits.
    const auto [clusters, visits] = NextVisits(continuation_params, true, 1);
    EXPECT_THAT(clusters, testing::ElementsAre(1));
    EXPECT_THAT(GetVisitIds(visits), testing::ElementsAre(2, 5));
    EXPECT_TRUE(continuation_params.exhausted_unclustered_visits);
    EXPECT_FALSE(continuation_params.exhausted_all_visits);
  }
}

TEST_P(HistoryClustersServiceTest, EndToEndWithBackend) {
  base::HistogramTester histogram_tester;
  AddHardcodedTestDataToHistoryService();

  base::RunLoop run_loop;
  auto run_loop_quit = run_loop.QuitClosure();

  const auto task = history_clusters_service_->QueryClusters(
      ClusteringRequestSource::kJourneysPage,
      /*begin_time=*/base::Time(),
      /*continuation_params=*/{},
      /*recluster=*/false,
      // This "expect" block is not run until after the fake response is
      // sent further down in this method.
      base::BindLambdaForTesting([&](std::vector<history::Cluster> clusters,
                                     QueryClustersContinuationParams) {
        ASSERT_EQ(clusters.size(), 2U);

        auto& cluster = clusters[0];
        auto& visits = cluster.visits;
        ASSERT_EQ(visits.size(), 2u);
        EXPECT_EQ(visits[0].annotated_visit.url_row.url(),
                  "https://github.com/");
        EXPECT_EQ(visits[0].annotated_visit.visit_row.visit_time,
                  GetHardcodedTestVisits()[1].visit_row.visit_time);
        EXPECT_EQ(visits[0].annotated_visit.url_row.title(),
                  u"Code Storage Title");
        EXPECT_FALSE(
            visits[0].annotated_visit.context_annotations.is_new_bookmark);
        EXPECT_TRUE(visits[0]
                        .annotated_visit.context_annotations
                        .is_existing_part_of_tab_group);
        EXPECT_FLOAT_EQ(visits[0].score, 0.5);

        EXPECT_EQ(visits[1].annotated_visit.url_row.url(),
                  "https://second-1-day-old-visit.com/");
        EXPECT_EQ(visits[1].annotated_visit.visit_row.visit_time,
                  GetHardcodedTestVisits()[4].visit_row.visit_time);
        EXPECT_EQ(visits[1].annotated_visit.url_row.title(),
                  u"second-1-day-old-visit");
        EXPECT_TRUE(
            visits[1].annotated_visit.context_annotations.is_new_bookmark);
        EXPECT_FALSE(visits[1]
                         .annotated_visit.context_annotations
                         .is_existing_part_of_tab_group);
        EXPECT_FLOAT_EQ(visits[1].score, 0.5);

        ASSERT_EQ(cluster.keyword_to_data_map.size(), 2u);
        EXPECT_TRUE(cluster.keyword_to_data_map.contains(u"apples"));
        EXPECT_TRUE(cluster.keyword_to_data_map.contains(u"Red Oranges"));

        cluster = clusters[1];
        visits = cluster.visits;
        ASSERT_EQ(visits.size(), 1u);
        EXPECT_EQ(visits[0].annotated_visit.url_row.url(),
                  "https://github.com/");
        EXPECT_EQ(visits[0].annotated_visit.visit_row.visit_time,
                  GetHardcodedTestVisits()[1].visit_row.visit_time);
        EXPECT_EQ(visits[0].annotated_visit.url_row.title(),
                  u"Code Storage Title");
        EXPECT_TRUE(cluster.keyword_to_data_map.empty());

        run_loop_quit.Run();
      }),
      HistoryClustersServiceTaskGetMostRecentClusters::Source::kWebUi);

  AwaitAndVerifyTestClusteringBackendRequest(ExpectSyncedVisits());

  std::vector<history::Cluster> clusters;
  clusters.push_back(
      history::Cluster(0,
                       {
                           test_clustering_backend_->GetVisitById(2),
                           test_clustering_backend_->GetVisitById(5),
                       },
                       {{u"apples", history::ClusterKeywordData()},
                        {u"Red Oranges", history::ClusterKeywordData()}},
                       /*should_show_on_prominent_ui_surfaces=*/true));
  clusters.push_back(
      history::Cluster(0,
                       {
                           test_clustering_backend_->GetVisitById(2),
                       },
                       {},
                       /*should_show_on_prominent_ui_surfaces=*/true));
  test_clustering_backend_->FulfillCallback(clusters);

  // Verify the callback is invoked.
  run_loop.Run();

  history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());
  histogram_tester.ExpectBucketCount(
      "History.Clusters.Backend.NumClustersReturned", 2, 1);
  histogram_tester.ExpectBucketCount(
      "History.Clusters.Backend.NumVisitsToCluster",
      ExpectSyncedVisits() ? 3 : 2, 1);
  histogram_tester.ExpectTotalCount(
      "History.Clusters.Backend.GetMostRecentClusters."
      "ComputeClustersLatency",
      1);
  histogram_tester.ExpectTotalCount(
      "History.Clusters.Backend.GetMostRecentClusters."
      "ComputeClustersLatency.WebUI",
      1);
  histogram_tester.ExpectTotalCount(
      "History.Clusters.Backend.GetMostRecentClusters."
      "ComputeClustersLatency.AllKeywordCacheRefresh",
      0);
  histogram_tester.ExpectTotalCount(
      "History.Clusters.Backend.GetMostRecentClusters."
      "GetMostRecentPersistedClustersLatency.ShortKeywordCacheRefresh",
      0);
}

TEST_P(HistoryClustersServiceTest, CompleteVisitContextAnnotationsIfReady) {
  auto test = [&](RecordingStatus status, bool expected_complete) {
    auto& incomplete_visit_context_annotations =
        history_clusters_service_->GetOrCreateIncompleteVisitContextAnnotations(
            0);
    incomplete_visit_context_annotations.url_row.set_id(1);
    incomplete_visit_context_annotations.visit_row.visit_id = 1;
    incomplete_visit_context_annotations.status = status;
    history_clusters_service_->CompleteVisitContextAnnotationsIfReady(0);
    EXPECT_NE(
        history_clusters_service_->HasIncompleteVisitContextAnnotations(0),
        expected_complete);
  };

  // Complete cases:
  {
    SCOPED_TRACE("Complete without UKM");
    test({true, true, true}, true);
  }
  {
    SCOPED_TRACE("Complete with UKM");
    test({true, true, true, true, true}, true);
  }

  // Incomplete without UKM cases:
  {
    SCOPED_TRACE("Incomplete, missing history rows");
    test({false, true}, false);
  }
  {
    SCOPED_TRACE("Incomplete, navigation hasn't ended");
    test({true}, false);
  }
  {
    SCOPED_TRACE("Incomplete, navigation end metrics haven't been recorded");
    test({true, true}, false);
  }

  // Incomplete with UKM cases:
  {
    SCOPED_TRACE("Incomplete, missing history rows");
    test({false, true, false, true, true}, false);
  }
  {
    SCOPED_TRACE("Incomplete, navigation hasn't ended");
    test({true, false, false, true, true}, false);
  }
  {
    SCOPED_TRACE("Incomplete, navigation end metrics haven't been recorded");
    test({true, true, false, true, true}, false);
  }
  {
    SCOPED_TRACE("Incomplete, UKM page end missing");
    test({true, true, true, true, false}, false);
  }

  auto test_dcheck = [&](RecordingStatus status) {
    auto& incomplete_visit_context_annotations =
        history_clusters_service_->GetOrCreateIncompleteVisitContextAnnotations(
            0);
    incomplete_visit_context_annotations.url_row.set_id(1);
    incomplete_visit_context_annotations.visit_row.visit_id = 1;
    incomplete_visit_context_annotations.status = status;
    EXPECT_DCHECK_DEATH(
        history_clusters_service_->CompleteVisitContextAnnotationsIfReady(0));
    EXPECT_TRUE(
        history_clusters_service_->HasIncompleteVisitContextAnnotations(0));
  };

  // Impossible cases:
  {
    SCOPED_TRACE(
        "Impossible, navigation end signals recorded before navigation ended");
    test_dcheck({true, false, true});
  }
  {
    SCOPED_TRACE(
        "Impossible, navigation end signals recorded before history rows");
    test_dcheck({false, true, true});
  }
  {
    SCOPED_TRACE("Impossible, unexpected UKM page end recorded");
    test_dcheck({false, false, false, false, true});
  }
}

class HistoryClustersServiceJourneysDisabledTest
    : public HistoryClustersServiceTestBase {
 public:
  HistoryClustersServiceJourneysDisabledTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{
            internal::kJourneys,
            internal::kPersistContextAnnotationsInHistoryDb,
        });
  }
};

TEST_F(HistoryClustersServiceJourneysDisabledTest,
       CompleteVisitContextAnnotationsIfReadyWhenFeatureDisabled) {
  // When the feature is disabled, the `IncompleteVisitContextAnnotations`
  // should be removed but not added to visits.
  auto& incomplete_visit_context_annotations =
      history_clusters_service_->GetOrCreateIncompleteVisitContextAnnotations(
          0);
  incomplete_visit_context_annotations.url_row.set_id(1);
  incomplete_visit_context_annotations.visit_row.visit_id = 1;
  incomplete_visit_context_annotations.status = {true, true, true};
  history_clusters_service_->CompleteVisitContextAnnotationsIfReady(0);
  EXPECT_FALSE(
      history_clusters_service_->HasIncompleteVisitContextAnnotations(0));
}

TEST_P(HistoryClustersServiceTest,
       CompleteVisitContextAnnotationsIfReadyWhenFeatureEnabled) {
  // When the feature is enabled, the `IncompleteVisitContextAnnotations`
  // should be removed and added to visits.
  auto& incomplete_visit_context_annotations =
      history_clusters_service_->GetOrCreateIncompleteVisitContextAnnotations(
          0);
  incomplete_visit_context_annotations.url_row.set_id(1);
  incomplete_visit_context_annotations.visit_row.visit_id = 1;
  incomplete_visit_context_annotations.status = {true, true, true};
  history_clusters_service_->CompleteVisitContextAnnotationsIfReady(0);
  EXPECT_FALSE(
      history_clusters_service_->HasIncompleteVisitContextAnnotations(0));
}

TEST_P(HistoryClustersServiceTest, DoesQueryMatchAnyCluster) {
  AddHardcodedTestDataToHistoryService();

  // Verify that initially, the test keyword doesn't match anything, but this
  // query should have kicked off a cache population request.
  EXPECT_FALSE(history_clusters_service_->DoesQueryMatchAnyCluster("apples"));

  std::vector<history::Cluster> clusters;
  clusters.push_back(history::Cluster(
      0,
      {
          GetHardcodedClusterVisit(5),
          GetHardcodedClusterVisit(2),
      },
      {{u"apples", history::ClusterKeywordData(
                       history::ClusterKeywordData::kEntity, 5.0f, {})},
       {u"oranges", history::ClusterKeywordData()},
       {u"z", history::ClusterKeywordData()},
       {u"apples bananas", history::ClusterKeywordData()}},
      /*should_show_on_prominent_ui_surfaces=*/true));
  clusters.push_back(history::Cluster(
      0,
      {
          GetHardcodedClusterVisit(5),
          GetHardcodedClusterVisit(2),
      },
      {
          {u"apples",
           history::ClusterKeywordData(
               history::ClusterKeywordData::kSearchTerms, 100.0f, {})},
      },
      /*should_show_on_prominent_ui_surfaces=*/true));
  clusters.push_back(
      history::Cluster(0,
                       {
                           GetHardcodedClusterVisit(5),
                           GetHardcodedClusterVisit(2),
                       },
                       {{u"sensitive", history::ClusterKeywordData()}},
                       /*should_show_on_prominent_ui_surfaces=*/false));
  clusters.push_back(
      history::Cluster(0,
                       {
                           GetHardcodedClusterVisit(5),
                       },
                       {{u"singlevisit", history::ClusterKeywordData()}},
                       /*should_show_on_prominent_ui_surfaces=*/true));

  // Hardcoded test visits span 3 days (1-day-old, 2-days-old, and 60-day-old).
  FlushKeywordRequests(clusters, 3);

  // Now the exact query should match the populated cache.
  const auto keyword_data =
      history_clusters_service_->DoesQueryMatchAnyCluster("apples");
  EXPECT_TRUE(keyword_data);
  // Its keyword data type is kSearchTerms as it has a higher score.
  EXPECT_EQ(keyword_data,
            history::ClusterKeywordData(
                history::ClusterKeywordData::kSearchTerms, 100.0f, {}));

  // Check that clusters that shouldn't be shown on prominent UI surfaces don't
  // have their keywords inserted into the keyword bag.
  EXPECT_FALSE(
      history_clusters_service_->DoesQueryMatchAnyCluster("sensitive"));

  // Ignore clusters with fewer than two visits.
  EXPECT_FALSE(
      history_clusters_service_->DoesQueryMatchAnyCluster("singlevisit"));

  // Too-short prefix queries rejected.
  EXPECT_FALSE(history_clusters_service_->DoesQueryMatchAnyCluster("ap"));

  // Single character exact queries are also rejected.
  EXPECT_FALSE(history_clusters_service_->DoesQueryMatchAnyCluster("z"));

  // Non-exact (substring) matches are rejected too.
  EXPECT_FALSE(history_clusters_service_->DoesQueryMatchAnyCluster("appl"));

  // Adding a second non-exact query word also should make it no longer match.
  EXPECT_FALSE(
      history_clusters_service_->DoesQueryMatchAnyCluster("apples oran"));

  // A multi-word phrase shouldn't be considered a match against two separate
  // keywords: "apples oranges" can't match keywords ["apples", "oranges"].
  EXPECT_FALSE(
      history_clusters_service_->DoesQueryMatchAnyCluster("apples oranges"));

  // But a multi-word phrases can still match against a keyword with multiple
  // words: "apples bananas" matches ["apples bananas"].
  EXPECT_TRUE(
      history_clusters_service_->DoesQueryMatchAnyCluster("apples bananas"));

  // Deleting a history entry should clear the keyword cache.
  history_service_->DeleteURLs({GURL{"https://google.com/"}});
  history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());
  EXPECT_FALSE(history_clusters_service_->DoesQueryMatchAnyCluster("apples"));

  // Visits now span 2 days (1-day-old and 60-day-old) since we deleted the only
  // 2-day-old visit.
  FlushKeywordRequests(clusters, 2);

  // The keyword cache should be repopulated.
  EXPECT_TRUE(history_clusters_service_->DoesQueryMatchAnyCluster("apples"));
}

TEST_P(HistoryClustersServiceTest, DoesQueryMatchAnyClusterSecondaryCache) {
  auto minutes_ago = [](int minutes) {
    return base::Time::Now() - base::Minutes(minutes);
  };

  // Set up the cache timestamps.
  history_clusters_service_test_api_->SetAllKeywordsCacheTimestamp(
      minutes_ago(60));
  history_clusters_service_test_api_->SetShortKeywordCacheTimestamp(
      minutes_ago(15));

  // Set up the visit timestamps.
  // Visits newer than both cache timestamps should be reclustered.
  AddIncompleteVisit(1, 1, minutes_ago(5));
  // Visits older than the secondary cache timestamp should be reclustered.
  AddIncompleteVisit(2, 2, minutes_ago(30));
  // Visits older than the primary cache timestamp should not be reclustered.
  AddIncompleteVisit(3, 3, minutes_ago(70));

  // Kick off cluster request and verify the correct visits are sent.
  EXPECT_FALSE(history_clusters_service_->DoesQueryMatchAnyCluster("peach"));
  test_clustering_backend_->WaitForGetClustersCall();
  std::vector<history::AnnotatedVisit> visits =
      test_clustering_backend_->LastClusteredVisits();
  EXPECT_THAT(GetVisitIds(visits), testing::ElementsAre(1, 2));

  // Send the cluster response and verify the keyword was cached.
  std::vector<history::Cluster> clusters2;
  clusters2.push_back(
      history::Cluster(0,
                       {
                           test_clustering_backend_->GetVisitById(1),
                           test_clustering_backend_->GetVisitById(2),
                       },
                       {{u"peach", history::ClusterKeywordData()},
                        {u"", history::ClusterKeywordData()}},
                       /*should_show_on_prominent_ui_surfaces=*/true));
  test_clustering_backend_->FulfillCallback(clusters2);
  history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());
  EXPECT_TRUE(history_clusters_service_->DoesQueryMatchAnyCluster("peach"));
}

TEST_P(HistoryClustersServiceTest, DoesURLMatchAnyClusterWithNoisyURLs) {
  Config config;
  config.omnibox_action_on_urls = true;
  config.omnibox_action_on_noisy_urls = true;
  SetConfigForTesting(config);

  AddHardcodedTestDataToHistoryService();

  // Verify that initially, the test URL doesn't match anything, but this
  // query should have kicked off a cache population request. This is the URL
  // for visit 5.
  EXPECT_FALSE(history_clusters_service_->DoesURLMatchAnyCluster(
      ComputeURLKeywordForLookup(GURL("https://second-1-day-old-visit.com/"))));

  std::vector<history::Cluster> clusters;
  clusters.push_back(history::Cluster(
      0,
      {
          GetHardcodedClusterVisit(5),
          GetHardcodedClusterVisit(
              /*visit_id=*/2, /*score=*/0.0, /*engagement_score=*/20.0),
      },
      {{u"apples", history::ClusterKeywordData()},
       {u"oranges", history::ClusterKeywordData()},
       {u"z", history::ClusterKeywordData()},
       {u"apples bananas", history::ClusterKeywordData()}},
      /*should_show_on_prominent_ui_surfaces=*/true));
  clusters.push_back(
      history::Cluster(0,
                       {
                           GetHardcodedClusterVisit(5),
                           GetHardcodedClusterVisit(2),
                       },
                       {{u"sensitive", history::ClusterKeywordData()}},
                       /*should_show_on_prominent_ui_surfaces=*/false));
  clusters.push_back(
      history::Cluster(0,
                       {
                           GetHardcodedClusterVisit(2),
                       },
                       {{u"singlevisit", history::ClusterKeywordData()}},
                       /*should_show_on_prominent_ui_surfaces=*/true));

  // Hardcoded test visits span 3 days (1-day-old, 2-days-old, and 60-day-old).
  FlushKeywordRequests(clusters, 3);

  // Now the exact query should match the populated cache.
  EXPECT_TRUE(history_clusters_service_->DoesURLMatchAnyCluster(
      ComputeURLKeywordForLookup(GURL("https://second-1-day-old-visit.com/"))));

  // Github should be shown since we are including visits from noisy URLs.
  EXPECT_TRUE(history_clusters_service_->DoesURLMatchAnyCluster(
      ComputeURLKeywordForLookup(GURL("https://github.com/"))));

  // Deleting a history entry should clear the keyword cache.
  history_service_->DeleteURLs({GURL{"https://google.com/"}});
  history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());
  EXPECT_FALSE(history_clusters_service_->DoesURLMatchAnyCluster(
      ComputeURLKeywordForLookup(GURL("https://second-1-day-old-visit.com/"))));

  // Visits now span 2 days (1-day-old and 60-day-old) since we deleted the only
  // 2-day-old visit.
  FlushKeywordRequests(clusters, 2);

  // The keyword cache should be repopulated.
  EXPECT_TRUE(history_clusters_service_->DoesURLMatchAnyCluster(
      ComputeURLKeywordForLookup(GURL("https://second-1-day-old-visit.com/"))));
}

TEST_P(HistoryClustersServiceTest, DoesURLMatchAnyClusterNoNoisyURLs) {
  Config config;
  config.omnibox_action_on_urls = true;
  config.omnibox_action_on_noisy_urls = false;
  SetConfigForTesting(config);

  AddHardcodedTestDataToHistoryService();

  // Verify that initially, the test URL doesn't match anything, but this
  // query should have kicked off a cache population request. This is the URL
  // for visit 5.
  EXPECT_FALSE(history_clusters_service_->DoesURLMatchAnyCluster(
      ComputeURLKeywordForLookup(GURL("https://second-1-day-old-visit.com/"))));

  std::vector<history::Cluster> clusters;
  clusters.push_back(history::Cluster(
      0,
      {
          GetHardcodedClusterVisit(5),
          GetHardcodedClusterVisit(
              /*visit_id=*/2, /*score=*/0.0, /*engagement_score=*/20.0),
      },
      {{u"apples", history::ClusterKeywordData()},
       {u"oranges", history::ClusterKeywordData()},
       {u"z", history::ClusterKeywordData()},
       {u"apples bananas", history::ClusterKeywordData()}},
      /*should_show_on_prominent_ui_surfaces=*/true));
  clusters.push_back(
      history::Cluster(0,
                       {
                           GetHardcodedClusterVisit(5),
                           GetHardcodedClusterVisit(2),
                       },
                       {{u"sensitive", history::ClusterKeywordData()}},
                       /*should_show_on_prominent_ui_surfaces=*/false));
  clusters.push_back(
      history::Cluster(0,
                       {
                           GetHardcodedClusterVisit(2),
                       },
                       {{u"singlevisit", history::ClusterKeywordData()}},
                       /*should_show_on_prominent_ui_surfaces=*/true));

  // Hardcoded test visits span 3 days (1-day-old, 2-days-old, and 60-day-old).
  FlushKeywordRequests(clusters, 3);

  // Now the exact query should match the populated cache.
  EXPECT_TRUE(history_clusters_service_->DoesURLMatchAnyCluster(
      ComputeURLKeywordForLookup(GURL("https://second-1-day-old-visit.com/"))));

  // Github should never be shown (highly-engaged for cluster 1, sensitive for
  // cluster 2, single visit cluster for cluster 3).
  EXPECT_FALSE(history_clusters_service_->DoesURLMatchAnyCluster(
      ComputeURLKeywordForLookup(GURL("https://github.com/"))));

  // Deleting a history entry should clear the keyword cache.
  history_service_->DeleteURLs({GURL{"https://google.com/"}});
  history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());
  EXPECT_FALSE(history_clusters_service_->DoesURLMatchAnyCluster(
      ComputeURLKeywordForLookup(GURL("https://second-1-day-old-visit.com/"))));

  // Visits now span 2 days (1-day-old and 60-day-old) since we deleted the only
  // 2-day-old visit.
  FlushKeywordRequests(clusters, 2);

  // The keyword cache should be repopulated.
  EXPECT_TRUE(history_clusters_service_->DoesURLMatchAnyCluster(
      ComputeURLKeywordForLookup(GURL("https://second-1-day-old-visit.com/"))));
}

class HistoryClustersServiceMaxKeywordsTest
    : public HistoryClustersServiceTestBase {
 public:
  HistoryClustersServiceMaxKeywordsTest() {
    // Set the max keyword phrases to 5.
    config_.is_journeys_enabled_no_locale_check = true;
    config_.max_keyword_phrases = 5;
    SetConfigForTesting(config_);
  }

 private:
  Config config_;
};

TEST_F(HistoryClustersServiceMaxKeywordsTest,
       DoesQueryMatchAnyClusterMaxKeywordPhrases) {
  base::HistogramTester histogram_tester;

  // Add visits.
  const auto yesterday = base::Time::Now() - base::Days(1);
  AddIncompleteVisit(1, 1, yesterday);
  AddIncompleteVisit(2, 2, yesterday);
  AddIncompleteVisit(3, 3, yesterday);
  AddIncompleteVisit(4, 4, yesterday);
  AddIncompleteVisit(5, 5, yesterday);
  AddIncompleteVisit(6, 6, yesterday);
  AddIncompleteVisit(7, 7, yesterday);

  // Create 4 clusters:
  std::vector<history::AnnotatedVisit> visits =
      test_clustering_backend_->LastClusteredVisits();
  std::vector<history::Cluster> clusters;
  // 1) A cluster with 4 phrases and 6 words. The next cluster's keywords should
  // also be cached since we have less than 5 phrases.
  clusters.push_back(
      history::Cluster(0, {{}, {}},
                       {{u"one", history::ClusterKeywordData()},
                        {u"two", history::ClusterKeywordData()},
                        {u"three", history::ClusterKeywordData()},
                        {u"four five six", history::ClusterKeywordData()}},
                       /*should_show_on_prominent_ui_surfaces=*/true));
  // 2) The 2nd cluster has only 1 visit. Since it's keywords won't be cached,
  // they should not affect the max.
  clusters.push_back(history::Cluster(
      0, {{}},
      {{u"ignored not cached", history::ClusterKeywordData()},
       {u"elephant penguin kangaroo", history::ClusterKeywordData()}},
      /*should_show_on_prominent_ui_surfaces=*/true));
  // 3) With this 3rd cluster, we'll have 5 phrases and 7 words. Now that we've
  // reached 5 phrases, the next cluster's keywords should not be cached.
  clusters.push_back(
      history::Cluster(0, {{}, {}}, {{u"seven", history::ClusterKeywordData()}},
                       /*should_show_on_prominent_ui_surfaces=*/true));
  // 4) The 4th cluster's keywords should not be cached since we've reached 5
  // phrases.
  clusters.push_back(
      history::Cluster(0, {{}, {}}, {{u"eight", history::ClusterKeywordData()}},
                       /*should_show_on_prominent_ui_surfaces=*/true));

  // Kick off cluster request.
  EXPECT_FALSE(history_clusters_service_->DoesQueryMatchAnyCluster("peach"));
  FlushKeywordRequests(clusters, 1);

  ASSERT_EQ(test_clustering_backend_->LastClusteredVisits().size(), 7u);

  // The 1st cluster's phrases should always be cached.
  EXPECT_TRUE(history_clusters_service_->DoesQueryMatchAnyCluster("one"));
  EXPECT_TRUE(
      history_clusters_service_->DoesQueryMatchAnyCluster("four five six"));
  // Phrases should be cached if we haven't reached 5 phrases even if we've
  // reached 5 words.
  EXPECT_TRUE(history_clusters_service_->DoesQueryMatchAnyCluster("seven"));
  // Phrases after the first 5 won't be cached.
  EXPECT_FALSE(history_clusters_service_->DoesQueryMatchAnyCluster("eight"));
  // Phrases of cluster's with 1 visit won't be cached.
  EXPECT_FALSE(history_clusters_service_->DoesQueryMatchAnyCluster("penguin"));

  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.KeywordCache.AllKeywordsCount", 5, 1);
  histogram_tester.ExpectTotalCount(
      "History.Clusters.Backend.KeywordCache.ShortKeywordsCount", 0);
}

}  // namespace

}  // namespace history_clusters
