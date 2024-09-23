// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/history_clusters_service.h"

#include <memory>
#include <string>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
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
#include "components/history_clusters/core/history_clusters_prefs.h"
#include "components/history_clusters/core/history_clusters_service_task_get_most_recent_clusters.h"
#include "components/history_clusters/core/history_clusters_service_task_update_clusters.h"
#include "components/history_clusters/core/history_clusters_service_test_api.h"
#include "components/history_clusters/core/history_clusters_types.h"
#include "components/history_clusters/core/history_clusters_util.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace history_clusters {

base::Time DaysAgo(int days) {
  return base::Time::Now() - base::Days(days);
}

// Trivial backend to allow us to specifically test just the service behavior.
class TestClusteringBackend : public ClusteringBackend {
 public:
  void GetClusters(ClusteringRequestSource clustering_request_source,
                   ClustersCallback callback,
                   std::vector<history::AnnotatedVisit> visits,
                   bool unused_requires_ui_and_triggerability) override {
    callback_ = std::move(callback);
    last_clustered_visits_ = visits;
    ASSERT_TRUE(wait_for_get_clusters_closure_)
        << "Unexpected `GetClusters()` called without "
           "`WaitForGetClustersCall()`.";
    std::move(wait_for_get_clusters_closure_).Run();
  }

  void GetClustersForUI(ClusteringRequestSource clustering_request_source,
                        QueryClustersFilterParams filter_params,
                        ClustersCallback callback,
                        std::vector<history::Cluster> clusters) override {
    callback_ = std::move(callback);
    last_clustered_clusters_ = clusters;

    std::move(wait_for_get_clusters_closure_).Run();
  }

  void GetClusterTriggerability(
      ClustersCallback callback,
      std::vector<history::Cluster> clusters) override {
    // TODO(b/259466296): Implement this when we incorporate the new method into
    //   `UpdateClusters()`.
  }

  void FulfillCallback(const std::vector<history::Cluster>& clusters) {
    std::move(callback_).Run(clusters);
  }

  const std::vector<history::AnnotatedVisit>& LastClusteredVisits() const {
    return last_clustered_visits_;
  }

  const std::vector<history::Cluster>& LastClusteredClusters() const {
    return last_clustered_clusters_;
  }

  // Fetches a scored visit by an ID. `visit_id` must be valid. This is a
  // convenience method used for constructing the fake response.
  history::ClusterVisit GetVisitById(int visit_id) {
    for (const auto& visit : last_clustered_visits_) {
      if (visit.visit_row.visit_id == visit_id)
        return AnnotatedVisitToClusterVisit(visit);
    }

    NOTREACHED_IN_MIGRATION()
        << "TestClusteringBackend::GetVisitById() could not find visit_id: "
        << visit_id;
    return {};
  }

  // Should be invoked before `GetClusters()` is invoked.
  void WaitForGetClustersCall() {
    base::RunLoop run_loop;
    wait_for_get_clusters_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  // Calls `WaitForGetClustersCall()`, verifies the clustered visits, and then
  // calls `FulfillCallback()`.
  void WaitExpectAndFulfillClustersCall(
      const std::vector<history::VisitID>& expected_clustered_visit_ids,
      const std::vector<history::Cluster>& fulfill_clusters) {
    WaitForGetClustersCall();
    EXPECT_THAT(GetVisitIds(LastClusteredVisits()),
                testing::ElementsAreArray(expected_clustered_visit_ids));
    FulfillCallback(fulfill_clusters);
  }

 private:
  base::OnceClosure wait_for_get_clusters_closure_;

  ClustersCallback callback_;
  std::vector<history::AnnotatedVisit> last_clustered_visits_;
  std::vector<history::Cluster> last_clustered_clusters_;
};

class HistoryClustersServiceTest : public testing::Test {
 public:
  HistoryClustersServiceTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME) {
    Config config;
    config.is_journeys_enabled_no_locale_check = true;
    // TODO(b/276488340): Update this test when non context clusterer code gets
    //   cleaned up.
    config.use_navigation_context_clusters = false;
    SetConfigForTesting(config);
  }

  void SetUp() override {
    CHECK(history_dir_.CreateUniqueTempDir());
    history_service_ =
        history::CreateHistoryService(history_dir_.GetPath(), true);
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    prefs::RegisterProfilePrefs(pref_service_->registry());

    ResetHistoryClustersServiceWithLocale("en-US");
  }

  void TearDown() override {
    // Give history a chance to flush out the task to avoid memory leaks.
    history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());

    history_clusters_service_.reset();
    pref_service_.reset();
    history_service_.reset();
  }

  HistoryClustersServiceTest(const HistoryClustersServiceTest&) = delete;
  HistoryClustersServiceTest& operator=(const HistoryClustersServiceTest&) =
      delete;

  void ResetHistoryClustersServiceWithLocale(const std::string& locale) {
    history_clusters_service_ = std::make_unique<HistoryClustersService>(
        locale, history_service_.get(),
        /*url_loader_factory=*/nullptr,
        /*engagement_score_provider=*/nullptr,
        /*template_url_service=*/nullptr,
        /*optimization_guide_decider=*/nullptr, pref_service_.get());
    history_clusters_service_test_api_ =
        std::make_unique<HistoryClustersServiceTestApi>(
            history_clusters_service_.get(), history_service_.get());
    auto test_backend = std::make_unique<TestClusteringBackend>();
    test_clustering_backend_ = test_backend.get();
    history_clusters_service_test_api_->SetClusteringBackendForTest(
        std::move(test_backend));

    // Kick off an initial `UpdateCluster()` request so that `QueryClusters()`
    // calls don't trigger an `UpdateCluster()` which would make testing very
    // complicated, since there'd be 2 async flows going on in parallel.
    history_clusters_service_->UpdateClusters();
    // `UpdateClusters()` will fetch visits to cluster from the history thread.
    // Block for the fetch to complete. There will be no subsequent model
    // request, since the visits will be empty.
    history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());
  }

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
    EXPECT_TRUE(add_page_args.url.is_valid())
        << " for URL \"" << add_page_args.url.possibly_invalid_spec() << "\"";
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
    visit.url_row.set_url(GURL("https://foo.com"));
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

  void AddCluster(std::vector<history::VisitID> visit_ids,
                  bool is_remote_cluster = false) {
    history::Cluster cluster = history::CreateCluster(visit_ids);
    if (is_remote_cluster) {
      cluster.originator_cache_guid = "otherdevice";
      cluster.originator_cluster_id = 1001 + visit_ids.front();
    }
    AddCluster(std::move(cluster));
  }

  void AddCluster(const history::Cluster& cluster) {
    base::CancelableTaskTracker task_tracker;
    history_service_->ReplaceClusters({}, {cluster}, base::DoNothing(),
                                      &task_tracker);
    history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());
  }

  // Verifies that the hardcoded visits were passed to the clustering backend.
  void AwaitAndVerifyTestClusteringBackendRequest() {
    test_clustering_backend_->WaitForGetClustersCall();

    std::vector<history::AnnotatedVisit> visits =
        test_clustering_backend_->LastClusteredVisits();

    // Visits 2, 3, and 5 are 1-day-old; visit 3 is a synced visit.
    ASSERT_EQ(visits.size(), 3u);

    auto& visit = visits[0];
    EXPECT_EQ(visit.visit_row.visit_id, 5);
    EXPECT_EQ(visit.visit_row.visit_time,
              GetHardcodedTestVisits()[4].visit_row.visit_time);
    EXPECT_EQ(visit.visit_row.visit_duration, base::Seconds(20));
    EXPECT_EQ(visit.url_row.url(), "https://second-1-day-old-visit.com/");
    EXPECT_EQ(visit.context_annotations.page_end_reason, 5);

    visit = visits[1];
    EXPECT_EQ(visit.visit_row.visit_id, 3);
    EXPECT_EQ(visit.visit_row.visit_time,
              GetHardcodedTestVisits()[2].visit_row.visit_time);
    EXPECT_EQ(visit.visit_row.visit_duration, base::Seconds(20));
    EXPECT_EQ(visit.url_row.url(), "https://synched-visit.com/");
    EXPECT_EQ(visit.context_annotations.page_end_reason, 5);

    visit = visits[2];
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
    QueryClustersFilterParams filter_params;
    // Including synced visits launched in early 2024.
    filter_params.include_synced_visits = true;
    const auto task = history_clusters_service_->QueryClusters(
        ClusteringRequestSource::kJourneysPage, std::move(filter_params),
        /*begin_time=*/base::Time(), continuation_params, /*recluster=*/false,
        base::BindLambdaForTesting(
            [&](std::vector<history::Cluster> clusters_temp,
                QueryClustersContinuationParams continuation_params_temp) {
              loop.Quit();
              clusters = clusters_temp;
              continuation_params = continuation_params_temp;
            }));

    // If we expect a clustering call, expect a request and return mirrored
    // clusters.
    if (expect_clustering_backend_call) {
      test_clustering_backend_->WaitForGetClustersCall();
      test_clustering_backend_->FulfillCallback(
          test_clustering_backend_->LastClusteredClusters());
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

  // A replacement for `HistoryClustersService::UpdateClusters()` that accepts a
  // callback.
  void UpdateClusters(base::OnceClosure callback) {
    DCHECK(!history_clusters_service_->update_clusters_task_ ||
           history_clusters_service_->update_clusters_task_->Done());
    history_clusters_service_->update_clusters_task_ =
        std::make_unique<HistoryClustersServiceTaskUpdateClusters>(
            history_clusters_service_->weak_ptr_factory_.GetWeakPtr(),
            IncompleteVisitMap{}, test_clustering_backend_,
            history_service_.get(), std::move(callback));
  }

  void SetJourneysVisible(bool visible) {
    pref_service_->SetBoolean(prefs::kVisible, visible);
  }

  void LoadCachesFromPrefs() {
    history_clusters_service_->LoadCachesFromPrefs();
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
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;

  std::unique_ptr<HistoryClustersService> history_clusters_service_;
  std::unique_ptr<HistoryClustersServiceTestApi>
      history_clusters_service_test_api_;

  // Non-owning pointer. The actual owner is `history_clusters_service_`.
  TestClusteringBackend* test_clustering_backend_;

  // Tracks the next available navigation ID to be associated with visits.
  int64_t next_navigation_id_ = 0;
};

TEST_F(HistoryClustersServiceTest, EligibleAndEnabledHistogramRecorded) {
  {
    base::HistogramTester histogram_tester;
    SetJourneysVisible(true);
    ResetHistoryClustersServiceWithLocale("en-US");
    histogram_tester.ExpectUniqueSample(
        "History.Clusters.JourneysEligibleAndEnabledAtSessionStart", true, 1);
  }

  {
    base::HistogramTester histogram_tester;
    SetJourneysVisible(false);
    ResetHistoryClustersServiceWithLocale("en-US");
    histogram_tester.ExpectUniqueSample(
        "History.Clusters.JourneysEligibleAndEnabledAtSessionStart", false, 1);
  }

  {
    base::HistogramTester histogram_tester;
    SetJourneysVisible(true);
    ResetHistoryClustersServiceWithLocale("garbagelocale");
    histogram_tester.ExpectTotalCount(
        "History.Clusters.JourneysEligibleAndEnabledAtSessionStart", 0);
  }
}

TEST_F(HistoryClustersServiceTest, HardCapOnVisitsFetchedFromHistory) {
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
      ClusteringRequestSource::kAllKeywordCacheRefresh,
      QueryClustersFilterParams(),
      /*begin_time=*/base::Time(), /*continuation_params=*/{},
      /*recluster=*/false,
      base::DoNothing());  // Only need to verify the correct request is sent

  test_clustering_backend_->WaitForGetClustersCall();
  history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());

  EXPECT_EQ(test_clustering_backend_->LastClusteredVisits().size(), 20U);
}

TEST_F(HistoryClustersServiceTest, QueryClusters_IncompleteAndPersistedVisits) {
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
    EXPECT_THAT(GetVisitIds(visits), testing::ElementsAre(5, 3, 2, 6));
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

TEST_F(HistoryClustersServiceTest,
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

TEST_F(HistoryClustersServiceTest,
       QueryClusters_PersistedClusters_UseNavigationContextClusters) {
  // Test the case where there are persisted clusters but no unclustered visits.

  Config config;
  // Set use navigation context clusters to false so the synthetic clusters can
  // be added without testing the history service observer logic that adds its
  // own clusters.
  config.use_navigation_context_clusters = false;
  SetConfigForTesting(config);

  // 2 persisted clusters on the same day.
  AddCompleteVisit(1, base::Time::Now() - base::Minutes(1));
  AddCompleteVisit(2, base::Time::Now() - base::Hours(1));
  AddCluster({1});
  AddCluster({2});

  // Another cluster with a gap. Should still be clustered with the others.
  AddCompleteVisit(3, DaysAgo(1));
  AddCluster({3});

  // Another cluster but it's remote. Should still be clustered with the others
  // if sync is enabled.
  AddCompleteVisit(4, DaysAgo(1));
  AddCluster({4}, /*is_remote_cluster=*/true);

  // Update config so that the new context clusters are used.
  Config new_config;
  new_config.use_navigation_context_clusters = true;
  SetConfigForTesting(new_config);

  QueryClustersContinuationParams continuation_params = {};

  {
    const auto [clusters, visits] =
        NextQueryClusters(continuation_params, true);
    std::vector<int64_t> expected_cluster_ids = {1, 2, 3, 4};
    ASSERT_THAT(GetClusterIds(clusters),
                testing::ElementsAreArray(expected_cluster_ids));
    EXPECT_THAT(GetVisitIds(clusters[0].visits), testing::ElementsAre(1));
    EXPECT_THAT(GetVisitIds(clusters[1].visits), testing::ElementsAre(2));
    EXPECT_THAT(GetVisitIds(clusters[2].visits), testing::ElementsAre(3));
    EXPECT_THAT(GetVisitIds(clusters[3].visits), testing::ElementsAre(4));
    EXPECT_THAT(GetVisitIds(visits), testing::ElementsAre());
    EXPECT_TRUE(continuation_params.exhausted_unclustered_visits);
    EXPECT_FALSE(continuation_params.exhausted_all_visits);
  }
  // The last query should set `exhausted_all_visits`.
  {
    const auto [clusters, visits] =
        NextQueryClusters(continuation_params, true);
    EXPECT_THAT(GetClusterIds(clusters), testing::ElementsAre());
    EXPECT_THAT(GetVisitIds(visits), testing::ElementsAre());
    EXPECT_TRUE(continuation_params.exhausted_unclustered_visits);
    EXPECT_TRUE(continuation_params.exhausted_all_visits);
  }
}

TEST_F(HistoryClustersServiceTest, QueryClusters_PersistedClusters_Today) {
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

TEST_F(HistoryClustersServiceTest, QueryClusters_PersistedClusters_MixedDay) {
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

TEST_F(HistoryClustersServiceTest, QueryVisits_OldestFirst) {
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
    EXPECT_THAT(GetVisitIds(visits), testing::ElementsAre(5, 3, 2));
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

TEST_F(HistoryClustersServiceTest, QueryClusteredVisits) {
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

TEST_F(HistoryClustersServiceTest, EndToEndWithBackend) {
  base::HistogramTester histogram_tester;
  AddHardcodedTestDataToHistoryService();

  base::RunLoop run_loop;

  const auto task = history_clusters_service_->QueryClusters(
      ClusteringRequestSource::kJourneysPage, QueryClustersFilterParams(),
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

        run_loop.Quit();
      }));

  AwaitAndVerifyTestClusteringBackendRequest();

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
      "History.Clusters.Backend.NumVisitsToCluster", 3, 1);
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

TEST_F(HistoryClustersServiceTest, CompleteVisitContextAnnotationsIfReady) {
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

TEST_F(HistoryClustersServiceTest,
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

class HistoryClustersServiceKeywordTest : public HistoryClustersServiceTest {
 public:
  HistoryClustersServiceKeywordTest() {
    scoped_feature_list_.InitAndEnableFeature(internal::kJourneys);
    // Explicitly set the default configuration for testing.
    Config config;
    SetConfigForTesting(config);
  }
};

TEST_F(HistoryClustersServiceKeywordTest, DoesQueryMatchAnyCluster) {
  AddHardcodedTestDataToHistoryService();

  AddCluster(history::Cluster(
      0,
      {
          GetHardcodedClusterVisit(5),
          GetHardcodedClusterVisit(2),
      },
      {{u"apples", history::ClusterKeywordData(
                       history::ClusterKeywordData::kEntity, 5.0f)},
       {u"oranges", history::ClusterKeywordData()},
       {u"z", history::ClusterKeywordData()},
       {u"apples bananas", history::ClusterKeywordData()}},
      /*should_show_on_prominent_ui_surfaces=*/true));
  AddCluster(history::Cluster(
      0,
      {
          GetHardcodedClusterVisit(5),
          GetHardcodedClusterVisit(2),
      },
      {
          {u"apples", history::ClusterKeywordData(
                          history::ClusterKeywordData::kSearchTerms, 100.0f)},
      },
      /*should_show_on_prominent_ui_surfaces=*/true));
  AddCluster(history::Cluster(0,
                              {
                                  GetHardcodedClusterVisit(5),
                                  GetHardcodedClusterVisit(2),
                              },
                              {{u"sensitive", history::ClusterKeywordData()}},
                              /*should_show_on_prominent_ui_surfaces=*/false));
  AddCluster(history::Cluster(0,
                              {
                                  GetHardcodedClusterVisit(5),
                              },
                              {{u"singlevisit", history::ClusterKeywordData()}},
                              /*should_show_on_prominent_ui_surfaces=*/true));
  auto hidden_visit = GetHardcodedClusterVisit(5);
  hidden_visit.interaction_state =
      history::ClusterVisit::InteractionState::kHidden;
  AddCluster(history::Cluster(0,
                              {
                                  hidden_visit,
                              },
                              {{u"hiddenvisit", history::ClusterKeywordData()}},
                              /*should_show_on_prominent_ui_surfaces=*/true));

  // Verify that initially, the test keyword doesn't match anything, but this
  // query should have kicked off a cache population request.
  {
    base::RunLoop loop;
    history_clusters_service_->set_keyword_cache_refresh_callback_for_testing(
        loop.QuitClosure());
    EXPECT_FALSE(history_clusters_service_->DoesQueryMatchAnyCluster("apples"));
    loop.Run();
  }

  // Now the exact query should match the populated cache.
  const auto keyword_data =
      history_clusters_service_->DoesQueryMatchAnyCluster("apples");
  EXPECT_TRUE(keyword_data);
  // Its keyword data type is kSearchTerms as it has a higher score.
  EXPECT_EQ(keyword_data,
            history::ClusterKeywordData(
                history::ClusterKeywordData::kSearchTerms, 100.0f));

  // Check that clusters that shouldn't be shown on prominent UI surfaces don't
  // have their keywords inserted into the keyword bag.
  EXPECT_FALSE(
      history_clusters_service_->DoesQueryMatchAnyCluster("sensitive"));

  // Ignore clusters with fewer than two visits.
  EXPECT_FALSE(
      history_clusters_service_->DoesQueryMatchAnyCluster("singlevisit"));

  // Ignore clusters with all hidden visits.
  EXPECT_FALSE(
      history_clusters_service_->DoesQueryMatchAnyCluster("hiddenvisit"));

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

  // Deleting a history entry should clear the keyword cache. But then it will
  // refresh the cache again.
  history_service_->DeleteURLs({GURL{"https://google.com/"}});
  history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());
  {
    base::RunLoop loop;
    history_clusters_service_->set_keyword_cache_refresh_callback_for_testing(
        loop.QuitClosure());
    EXPECT_FALSE(history_clusters_service_->DoesQueryMatchAnyCluster("apples"));
    loop.Run();
  }

  // The keyword cache should be repopulated.
  EXPECT_TRUE(history_clusters_service_->DoesQueryMatchAnyCluster("apples"));
}

TEST_F(HistoryClustersServiceKeywordTest,
       DoesQueryMatchAnyClusterSecondaryCacheNavigationContextClusters) {
  // Seed some visits and clusters.
  const auto today = base::Time::Now() - base::Minutes(30);

  AddCompleteVisit(1, today);
  AddCompleteVisit(2, today);

  AddCompleteVisit(3, today - base::Minutes(10));
  AddCompleteVisit(4, today - base::Minutes(11));

  base::CancelableTaskTracker task_tracker;
  history::Cluster cluster = history::CreateCluster({1, 2});
  cluster.cluster_id = 1;
  cluster.keyword_to_data_map = {{u"peach", history::ClusterKeywordData()},
                                 {u"", history::ClusterKeywordData()}};
  cluster.should_show_on_prominent_ui_surfaces = true;

  history::Cluster cluster2 = history::CreateCluster({3, 4});
  cluster2.cluster_id = 2;
  cluster2.originator_cache_guid = "remotedevice";
  cluster2.originator_cluster_id = 1000;
  cluster2.keyword_to_data_map = {{u"remote", history::ClusterKeywordData()}};
  cluster2.should_show_on_prominent_ui_surfaces = true;
  history_service_->ReplaceClusters({}, {cluster, cluster2}, base::DoNothing(),
                                    &task_tracker);
  history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());

  auto minutes_ago = [](int minutes) {
    return base::Time::Now() - base::Minutes(minutes);
  };

  // Set up the cache timestamps.
  history_clusters_service_test_api_->SetAllKeywordsCacheTimestamp(
      minutes_ago(60));
  history_clusters_service_test_api_->SetShortKeywordCacheTimestamp(
      minutes_ago(15));

  // Kick off cluster request and populate the in-memory cache.
  {
    base::RunLoop loop;
    history_clusters_service_->set_keyword_cache_refresh_callback_for_testing(
        loop.QuitClosure());
    EXPECT_FALSE(history_clusters_service_->DoesQueryMatchAnyCluster("peach"));
    EXPECT_FALSE(history_clusters_service_->DoesQueryMatchAnyCluster("remote"));
    loop.Run();
  }

  EXPECT_TRUE(history_clusters_service_->DoesQueryMatchAnyCluster("peach"));
  EXPECT_TRUE(history_clusters_service_->DoesQueryMatchAnyCluster("remote"));
}

TEST_F(HistoryClustersServiceKeywordTest, LoadCachesFromPrefs) {
  AddHardcodedTestDataToHistoryService();
  AddCluster(history::Cluster(
      0,
      {
          GetHardcodedClusterVisit(5),
          GetHardcodedClusterVisit(2),
      },
      {{u"apples", history::ClusterKeywordData(
                       history::ClusterKeywordData::kEntity, 5.0f)},
       {u"oranges", history::ClusterKeywordData(
                        history::ClusterKeywordData::kSearchTerms, 100.0f)},
       {u"z", history::ClusterKeywordData()},
       {u"apples bananas", history::ClusterKeywordData()}},
      /*should_show_on_prominent_ui_surfaces=*/true));

  // Refresh the keyword cache.
  {
    base::RunLoop loop;
    history_clusters_service_->set_keyword_cache_refresh_callback_for_testing(
        loop.QuitClosure());
    EXPECT_FALSE(history_clusters_service_->DoesQueryMatchAnyCluster("apples"));
    loop.Run();
  }

  // Now it's populated in memory (after the first call).
  const auto keyword_data =
      history_clusters_service_->DoesQueryMatchAnyCluster("apples");
  EXPECT_TRUE(keyword_data);

  // Empty the cache artificially to simulate a process restart.
  history_clusters_service_test_api_->SetAllKeywordsCache({});
  EXPECT_FALSE(history_clusters_service_->DoesQueryMatchAnyCluster("apples"));

  LoadCachesFromPrefs();

  const auto apples_keyword_data =
      history_clusters_service_->DoesQueryMatchAnyCluster("apples");
  EXPECT_TRUE(apples_keyword_data);
  EXPECT_EQ(
      apples_keyword_data,
      history::ClusterKeywordData(history::ClusterKeywordData::kEntity, 5.0f));
  const auto oranges_keyword_data =
      history_clusters_service_->DoesQueryMatchAnyCluster("oranges");
  EXPECT_TRUE(oranges_keyword_data);
  EXPECT_EQ(oranges_keyword_data,
            history::ClusterKeywordData(history::ClusterKeywordData(
                history::ClusterKeywordData::kSearchTerms, 100.0f)));
  EXPECT_TRUE(
      history_clusters_service_->DoesQueryMatchAnyCluster("apples bananas"));
}

TEST_F(HistoryClustersServiceKeywordTest, LoadSecondaryCachesFromPrefs) {
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
  auto visit = GetHardcodedTestVisits()[0];
  visit.visit_row.visit_time = minutes_ago(5);
  AddCompleteVisit(visit);
  visit = GetHardcodedTestVisits()[1];
  visit.visit_row.visit_time = minutes_ago(10);
  AddCompleteVisit(visit);

  AddCluster(history::Cluster(
      0,
      {
          GetHardcodedClusterVisit(1),
          GetHardcodedClusterVisit(2),
      },
      {{u"peach", history::ClusterKeywordData(
                      history::ClusterKeywordData::kEntity, 13.0f)},
       {u"", history::ClusterKeywordData()}},
      /*should_show_on_prominent_ui_surfaces=*/true));

  // Kick off the initial request, which should return false, but will populate
  // the in-memory caches.
  {
    base::RunLoop loop;
    history_clusters_service_->set_keyword_cache_refresh_callback_for_testing(
        loop.QuitClosure());
    EXPECT_FALSE(history_clusters_service_->DoesQueryMatchAnyCluster("peach"));
    loop.Run();
  }

  EXPECT_TRUE(history_clusters_service_->DoesQueryMatchAnyCluster("peach"));

  // Verify the keyword is in the short cache specifically.
  history_clusters_service_test_api_->SetAllKeywordsCache({});
  EXPECT_TRUE(history_clusters_service_->DoesQueryMatchAnyCluster("peach"));

  // Empty the cache artificially to simulate a process restart.
  history_clusters_service_test_api_->SetShortKeywordCache({});
  EXPECT_FALSE(history_clusters_service_->DoesQueryMatchAnyCluster("peach"));

  LoadCachesFromPrefs();
  const auto peach_keyword_data =
      history_clusters_service_->DoesQueryMatchAnyCluster("peach");
  EXPECT_EQ(peach_keyword_data,
            history::ClusterKeywordData(history::ClusterKeywordData(
                history::ClusterKeywordData::kEntity, 13.0f)));
}

TEST_F(HistoryClustersServiceKeywordTest,
       DoesQueryMatchAnyClusterMaxKeywordPhrases) {
  // For this test only, set an artificially low `max_keyword_phrases`.
  Config config;
  config.max_keyword_phrases = 5;
  SetConfigForTesting(config);

  base::HistogramTester histogram_tester;

  AddHardcodedTestDataToHistoryService();

  // Create 4 clusters:
  history::ClusterVisit cluster_visit;
  cluster_visit.score = .5;
  // 1) A cluster with 4 phrases and 6 words. The next cluster's keywords should
  // also be cached since we have less than 5 phrases.
  AddCluster(history::Cluster(
      0, {GetHardcodedClusterVisit(1), GetHardcodedClusterVisit(5)},
      {{u"one", history::ClusterKeywordData()},
       {u"two", history::ClusterKeywordData()},
       {u"three", history::ClusterKeywordData()},
       {u"four five six", history::ClusterKeywordData()}},
      /*should_show_on_prominent_ui_surfaces=*/true));
  // 2) The 2nd cluster has only 1 visit. Since it's keywords won't be cached,
  // they should not affect the max.
  AddCluster(history::Cluster(
      0, {{GetHardcodedClusterVisit(1)}},
      {{u"ignored not cached", history::ClusterKeywordData()},
       {u"elephant penguin kangaroo", history::ClusterKeywordData()}},
      /*should_show_on_prominent_ui_surfaces=*/true));
  // 3) With this 3rd cluster, we'll have 5 phrases and 7 words. Now that we've
  // reached 5 phrases, the next cluster's keywords should not be cached.
  AddCluster(history::Cluster(
      0, {GetHardcodedClusterVisit(1), GetHardcodedClusterVisit(5)},
      {{u"seven", history::ClusterKeywordData()}},
      /*should_show_on_prominent_ui_surfaces=*/true));
  // 4) The 4th cluster's keywords should not be cached since we've reached 5
  // phrases.
  AddCluster(history::Cluster(
      0, {GetHardcodedClusterVisit(1), GetHardcodedClusterVisit(5)},
      {{u"eight", history::ClusterKeywordData()}},
      /*should_show_on_prominent_ui_surfaces=*/true));

  // Kick off cluster request and populate in-memory caches.
  {
    base::RunLoop loop;
    history_clusters_service_->set_keyword_cache_refresh_callback_for_testing(
        loop.QuitClosure());
    EXPECT_FALSE(history_clusters_service_->DoesQueryMatchAnyCluster("peach"));
    loop.Run();
  }

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

class HistoryClustersServiceJourneysDisabledTest
    : public HistoryClustersServiceTest {
 public:
  HistoryClustersServiceJourneysDisabledTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{
            internal::kJourneys,
            internal::kPersistContextAnnotationsInHistoryDb,
        });

    Config config;
    config.is_journeys_enabled_no_locale_check = false;
    SetConfigForTesting(config);
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

TEST_F(HistoryClustersServiceJourneysDisabledTest, QueryClusters) {
  // Create 5 persisted visits with visit times 2, 1, 1, 60, and 1 days ago.
  AddHardcodedTestDataToHistoryService();

  QueryClustersContinuationParams continuation_params = {};
  continuation_params.continuation_time = base::Time::Now();

  const auto [clusters, visits] = NextQueryClusters(
      continuation_params, /*expect_clustering_backend_call=*/false);
  EXPECT_TRUE(clusters.empty());
}

TEST_F(HistoryClustersServiceTest, UpdateClusters_Sparse) {
  // Test the case where visits day distribution is wider than
  // `persist_clusters_recluster_window_days`; i.e. no reclustering occurs.
  Config config;
  // TODO(b/276488340): Update this test when non context clusterer code gets
  //   cleaned up.
  config.use_navigation_context_clusters = false;
  config.max_persisted_clusters_to_fetch = 10;
  config.max_persisted_cluster_visits_to_fetch_soft_cap = 5;
  config.persist_clusters_recluster_window_days = 2;
  SetConfigForTesting(config);

  // Create unclustered visits.
  AddCompleteVisit(1, DaysAgo(15));
  AddCompleteVisit(2, DaysAgo(12));
  AddCompleteVisit(3, DaysAgo(9));
  AddCompleteVisit(4, DaysAgo(6));

  {
    // The 1st call should cluster the 4 visits in 4 batches.
    base::RunLoop run_loop;
    UpdateClusters(run_loop.QuitClosure());

    // Expect 4 clustering calls, each for 1 visit.
    for (int id = 1; id <= 4; ++id) {
      test_clustering_backend_->WaitExpectAndFulfillClustersCall(
          {id}, {history::CreateCluster({id})});
    }
    run_loop.Run();

    // Expect the visits to be clustered into 4 clusters.
    QueryClustersContinuationParams continuation_params = {};
    const auto clusters = NextQueryClusters(continuation_params, false).first;
    EXPECT_THAT(GetClusterIds(clusters), testing::ElementsAre(4, 3, 2, 1));
    for (const auto& cluster : clusters) {
      EXPECT_THAT(GetVisitIds(cluster.visits),
                  testing::ElementsAre(cluster.cluster_id));
    }
  }

  {
    // A 2nd call shouldn't cluster anything.
    base::RunLoop run_loop;
    UpdateClusters(run_loop.QuitClosure());
    run_loop.Run();
  }

  {
    // New visits should be clustered.
    base::RunLoop run_loop;
    AddCompleteVisit(5, DaysAgo(3));
    UpdateClusters(run_loop.QuitClosure());
    test_clustering_backend_->WaitExpectAndFulfillClustersCall(
        {5}, {history::CreateCluster({5})});
    run_loop.Run();
  }

  {
    // An update call without new visits shouldn't cluster anything. Omitting
    // the backend wait ensures the test would crash if a clustering request
    // were made.
    base::RunLoop run_loop;
    UpdateClusters(run_loop.QuitClosure());
    run_loop.Run();
  }

  // The run loops ensure the update task is complete. We need 1 more history
  // block to ensure the update task is flushed out of the history service
  // queue.
  history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());
}

TEST_F(HistoryClustersServiceTest, UpdateClusters_Reclustering) {
  // Test the case where visits day distribution is denser than
  // `persist_clusters_recluster_window_days`; i.e. reclustering occurs.
  Config config;
  // TODO(b/276488340): Update this test when non context clusterer code gets
  //   cleaned up.
  config.use_navigation_context_clusters = false;
  config.max_persisted_clusters_to_fetch = 10;
  config.max_persisted_cluster_visits_to_fetch_soft_cap = 5;
  config.persist_clusters_recluster_window_days = 1;
  SetConfigForTesting(config);

  // Create unclustered visits.
  AddCompleteVisit(1, DaysAgo(16));
  AddCompleteVisit(2, DaysAgo(15));
  AddCompleteVisit(3, DaysAgo(14));
  AddCompleteVisit(4, DaysAgo(13));
  AddCompleteVisit(5, DaysAgo(12));
  AddCompleteVisit(6, DaysAgo(11));
  AddCompleteVisit(7, DaysAgo(10));
  AddCompleteVisit(8, DaysAgo(9));
  AddCompleteVisit(9, DaysAgo(8));

  {
    base::RunLoop run_loop;
    UpdateClusters(run_loop.QuitClosure());

    // The 1st 3 requests return 1-visit clusters. So the visits clustered
    // should be just the current and last visit. This creates cluster 1.
    test_clustering_backend_->WaitExpectAndFulfillClustersCall(
        {1}, {history::CreateCluster({1})});
    // This recreates cluster 1, and creates cluster 2.
    test_clustering_backend_->WaitExpectAndFulfillClustersCall(
        {2, 1}, {history::CreateCluster({1}), history::CreateCluster({2})});
    // This recreates cluster 2, and creates cluster 3.
    test_clustering_backend_->WaitExpectAndFulfillClustersCall(
        {3, 2}, {history::CreateCluster({2}), history::CreateCluster({3})});

    // When older than 1 day visits are clustered with 1 day old visits, they
    // too should be reclustered. This recreates cluster 3.
    test_clustering_backend_->WaitExpectAndFulfillClustersCall(
        {4, 3}, {history::CreateCluster({3, 4})});
    // This recreates cluster 3.
    test_clustering_backend_->WaitExpectAndFulfillClustersCall(
        {5, 4, 3}, {history::CreateCluster({3, 4, 5})});
    // This recreates cluster 3.
    test_clustering_backend_->WaitExpectAndFulfillClustersCall(
        {6, 5, 4, 3}, {history::CreateCluster({3, 4, 5, 6})});

    // When clusters no longer contain visits 1 day old, they should not be
    // reclustered.
    // This recreates cluster 3, and creates cluster 4.
    test_clustering_backend_->WaitExpectAndFulfillClustersCall(
        {7, 6, 5, 4, 3},
        {history::CreateCluster({3, 7}), history::CreateCluster({4, 5, 6})});
    // This deletes cluster 3, and creates clusters 5 & 6.
    test_clustering_backend_->WaitExpectAndFulfillClustersCall(
        {8, 7, 3},
        {history::CreateCluster({3, 8}), history::CreateCluster({7})});
    // This deletes cluster 5 and creates cluster 7.
    test_clustering_backend_->WaitExpectAndFulfillClustersCall(
        {9, 8, 3}, {history::CreateCluster({3, 8, 9})});
    run_loop.Run();
  }

  // There're now 5 clusters:
  // ID: 1, visits: 1
  // ID: 2, visits: 2
  // ID: 4, visits: 4, 5, 6
  // ID: 6, visits: 7
  // ID: 7, visits: 3, 8, 9

  // Check the final clusters. 1st request should get the last 3 clusters due to
  // `max_persisted_cluster_visits_to_fetch_soft_cap`.
  QueryClustersContinuationParams continuation_params = {};
  auto clusters = NextQueryClusters(continuation_params, false).first;
  EXPECT_THAT(GetClusterIds(clusters), testing::ElementsAre(13, 12, 10));
  EXPECT_THAT(GetVisitIds(clusters[0].visits), testing::ElementsAre(9, 8, 3));
  EXPECT_THAT(GetVisitIds(clusters[1].visits), testing::ElementsAre(7));
  EXPECT_THAT(GetVisitIds(clusters[2].visits), testing::ElementsAre(6, 5, 4));

  clusters = NextQueryClusters(continuation_params, false).first;
  EXPECT_THAT(GetClusterIds(clusters), testing::ElementsAre(4, 2));
  EXPECT_THAT(GetVisitIds(clusters[0].visits), testing::ElementsAre(2));
  EXPECT_THAT(GetVisitIds(clusters[1].visits), testing::ElementsAre(1));

  clusters = NextQueryClusters(continuation_params, false).first;
  EXPECT_THAT(GetClusterIds(clusters), testing::ElementsAre());

  // Test `max_persisted_clusters_to_fetch`.
  config.max_persisted_clusters_to_fetch = 2;
  SetConfigForTesting(config);
  continuation_params = {};
  clusters = NextQueryClusters(continuation_params, false).first;
  EXPECT_THAT(GetClusterIds(clusters), testing::ElementsAre(13, 12));

  {
    // A 2nd call shouldn't cluster anything.
    base::RunLoop run_loop;
    UpdateClusters(run_loop.QuitClosure());
    run_loop.Run();
  }

  {
    // New visits older than the clustered visits should not be clustered.
    base::RunLoop run_loop;
    AddCompleteVisit(10, DaysAgo(8) - base::Minutes(1));
    UpdateClusters(run_loop.QuitClosure());
    run_loop.Run();
  }

  {
    // New visits newer than the clustered visits should be clustered. Visit 10
    // is lost as it's unclustered yet older than the clustering boundary.
    // Changing this behavior wouldn't be wrong, but unnecessary currently.
    base::RunLoop run_loop;
    AddCompleteVisit(11, DaysAgo(8) + base::Minutes(1));
    UpdateClusters(run_loop.QuitClosure());
    test_clustering_backend_->WaitExpectAndFulfillClustersCall(
        {11, 9, 8, 3}, {history::CreateCluster({3, 8, 9, 11})});
    run_loop.Run();
  }

  {
    // Likewise for new visits on new days.
    base::RunLoop run_loop;
    AddCompleteVisit(12, DaysAgo(7));
    UpdateClusters(run_loop.QuitClosure());
    test_clustering_backend_->WaitExpectAndFulfillClustersCall(
        {12, 11, 9, 8, 3}, {history::CreateCluster({12, 11, 9, 8, 3})});
    run_loop.Run();
  }

  {
    // An update call without new visits shouldn't cluster anything. Omitting
    // the backend wait ensures the test would crash if a clustering request
    // were made.
    base::RunLoop run_loop;
    UpdateClusters(run_loop.QuitClosure());
    run_loop.Run();
  }

  // The run loops ensure the update task is complete. We need 1 more history
  // block to ensure the update task is flushed out of the history service
  // queue.
  history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());
}

TEST_F(HistoryClustersServiceTest, UpdateClusters_ReclusterMultipleClusters) {
  // Test the case where there are multiple clusters reconsulted in the same
  // batch.
  Config config;
  // TODO(b/276488340): Update this test when non context clusterer code gets
  //   cleaned up.
  config.use_navigation_context_clusters = false;
  config.max_visits_to_cluster = 5;
  config.max_persisted_clusters_to_fetch = 100;
  config.max_persisted_cluster_visits_to_fetch_soft_cap = 100;
  config.persist_clusters_recluster_window_days = 1;
  SetConfigForTesting(config);

  // Create unclustered visits.
  AddCompleteVisit(1, DaysAgo(10));
  AddCompleteVisit(2, DaysAgo(10));
  AddCompleteVisit(3, DaysAgo(10));
  AddCompleteVisit(4, DaysAgo(10));
  AddCompleteVisit(5, DaysAgo(10));
  AddCompleteVisit(6, DaysAgo(10));
  AddCompleteVisit(7, DaysAgo(9));
  AddCompleteVisit(8, DaysAgo(9));
  AddCompleteVisit(9, DaysAgo(9));
  AddCompleteVisit(10, DaysAgo(8));

  {
    base::RunLoop run_loop;
    UpdateClusters(run_loop.QuitClosure());

    test_clustering_backend_->WaitExpectAndFulfillClustersCall(
        {6, 5, 4, 3, 2},
        {history::CreateCluster({2}), history::CreateCluster({3, 4}),
         history::CreateCluster({5, 6})});
    test_clustering_backend_->WaitExpectAndFulfillClustersCall(
        {9, 8, 7, 2},
        {history::CreateCluster({2, 7}), history::CreateCluster({8, 9})});
    test_clustering_backend_->WaitExpectAndFulfillClustersCall(
        {10, 7, 2, 9, 8}, {history::CreateCluster({2, 7, 8, 9, 10})});
    run_loop.Run();
  }

  // Check the final clusters.
  QueryClustersContinuationParams continuation_params = {};
  auto clusters = NextQueryClusters(continuation_params, false).first;
  EXPECT_THAT(GetClusterIds(clusters), testing::ElementsAre(6, 2, 3));
  EXPECT_THAT(GetVisitIds(clusters[0].visits),
              testing::ElementsAre(10, 9, 8, 7, 2));
  EXPECT_THAT(GetVisitIds(clusters[1].visits), testing::ElementsAre(4, 3));
  EXPECT_THAT(GetVisitIds(clusters[2].visits), testing::ElementsAre(6, 5));

  clusters = NextQueryClusters(continuation_params, false).first;
  EXPECT_THAT(GetClusterIds(clusters), testing::ElementsAre());

  {
    // An update call without new visits shouldn't cluster anything. Omitting
    // the backend wait ensures the test would crash if a clustering request
    // were made.
    base::RunLoop run_loop;
    UpdateClusters(run_loop.QuitClosure());
    run_loop.Run();
  }

  // The run loops ensure the update task is complete. We need 1 more history
  // block to ensure the update task is flushed out of the history service
  // queue.
  history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());
}

TEST_F(HistoryClustersServiceTest, UpdateClusters_PopularDay) {
  // Test the case there are more visits than `max_visits_to_cluster` in a day.
  Config config;
  // TODO(b/276488340): Update this test when non context clusterer code gets
  //   cleaned up.
  config.use_navigation_context_clusters = false;
  config.max_visits_to_cluster = 3;
  config.persist_clusters_recluster_window_days = 1;
  SetConfigForTesting(config);

  // Create unclustered visits 4 days old.
  AddCompleteVisit(1, DaysAgo(4));
  AddCompleteVisit(2, DaysAgo(4));
  AddCompleteVisit(3, DaysAgo(4));
  AddCompleteVisit(4, DaysAgo(4));
  AddCompleteVisit(5, DaysAgo(4));
  // Create unclustered visits 3 days old.
  AddCompleteVisit(6, DaysAgo(3));
  AddCompleteVisit(7, DaysAgo(3));
  AddCompleteVisit(8, DaysAgo(3));
  AddCompleteVisit(9, DaysAgo(3));

  {
    base::RunLoop run_loop;
    UpdateClusters(run_loop.QuitClosure());

    test_clustering_backend_->WaitExpectAndFulfillClustersCall(
        {5, 4, 3}, {history::CreateCluster({3, 4, 5})});
    // Should not include visits from the previous day when the current day
    // already has more than `max_visits_to_cluster` visits.
    test_clustering_backend_->WaitExpectAndFulfillClustersCall(
        {9, 8, 7}, {history::CreateCluster({7, 8, 9})});
    run_loop.Run();
  }

  QueryClustersContinuationParams continuation_params = {};
  auto clusters = NextQueryClusters(continuation_params, false).first;
  EXPECT_THAT(GetClusterIds(clusters), testing::ElementsAre(2, 1));
  EXPECT_THAT(GetVisitIds(clusters[0].visits), testing::ElementsAre(9, 8, 7));
  EXPECT_THAT(GetVisitIds(clusters[1].visits), testing::ElementsAre(5, 4, 3));

  clusters = NextQueryClusters(continuation_params, false).first;
  EXPECT_THAT(GetClusterIds(clusters), testing::ElementsAre());

  {
    // A 2nd call shouldn't cluster anything.
    base::RunLoop run_loop;
    UpdateClusters(run_loop.QuitClosure());
    run_loop.Run();
  }

  {
    // A new visit on the day with more than `max_visits_to_cluster` visits
    // should be clustered without reclustering the existing visit.
    base::RunLoop run_loop;
    AddCompleteVisit(10, DaysAgo(3) + base::Minutes(1));
    UpdateClusters(run_loop.QuitClosure());
    test_clustering_backend_->WaitExpectAndFulfillClustersCall(
        {10}, {history::CreateCluster({10})});
    run_loop.Run();
  }

  {
    // An update call without new visits shouldn't cluster anything. Omitting
    // the backend wait ensures the test would crash if a clustering request
    // were made.
    base::RunLoop run_loop;
    UpdateClusters(run_loop.QuitClosure());
    run_loop.Run();
  }

  // The run loops ensure the update task is complete. We need 1 more history
  // block to ensure the update task is flushed out of the history service
  // queue.
  history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());
}

}  // namespace history_clusters
