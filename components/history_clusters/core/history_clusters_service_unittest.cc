// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/history_clusters_service.h"

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/history/core/browser/history_context.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/history_clusters/core/clustering_backend.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/core/history_clusters_service_test_api.h"
#include "components/history_clusters/core/history_clusters_types.h"
#include "components/history_clusters/core/history_clusters_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace history_clusters {

namespace {

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
  history::ClusterVisit GetVisitById(int visit_id,
                                     float score = 0.5,
                                     int engagement_score = 0) {
    for (auto& visit : last_clustered_visits_) {
      if (visit.visit_row.visit_id == visit_id) {
        history::ClusterVisit cluster_visit;
        cluster_visit.annotated_visit = visit;
        cluster_visit.normalized_url = visit.url_row.url();
        cluster_visit.score = score;
        cluster_visit.engagement_score = engagement_score;
        return cluster_visit;
      }
    }

    NOTREACHED() << "TestClusteringBackend::GetVisitById "
                 << "could not find visit_id: " << visit_id;
    return history::ClusterVisit();
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
        /*engagement_score_provider=*/nullptr,
        /*url_loader_factory=*/nullptr);

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
    history::ContextID context_id = reinterpret_cast<history::ContextID>(1);

    for (auto& visit : GetHardcodedTestVisits()) {
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
          history_clusters_service_
              ->GetOrCreateIncompleteVisitContextAnnotations(
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

  // Verifies that the hardcoded visits were passed to the clustering backend.
  void AwaitAndVerifyTestClusteringBackendRequest() {
    test_clustering_backend_->WaitForGetClustersCall();

    std::vector<history::AnnotatedVisit> visits =
        test_clustering_backend_->LastClusteredVisits();

    // Visits 2, 3, and 5 are 1-day-old; visit 3 is a synced visit and therefore
    // excluded.
    ASSERT_EQ(visits.size(), 2u);

    auto& visit = visits[0];
    EXPECT_EQ(visit.visit_row.visit_id, 5);
    EXPECT_EQ(visit.visit_row.visit_time,
              GetHardcodedTestVisits()[4].visit_row.visit_time);
    EXPECT_EQ(visit.visit_row.visit_duration, base::Seconds(20));
    EXPECT_EQ(visit.url_row.url(), "https://second-1-day-old-visit.com/");
    EXPECT_EQ(visit.context_annotations.page_end_reason, 5);

    visit = visits[1];
    EXPECT_EQ(visit.visit_row.visit_id, 2);
    EXPECT_EQ(visit.visit_row.visit_time,
              GetHardcodedTestVisits()[1].visit_row.visit_time);
    EXPECT_EQ(visit.visit_row.visit_duration, base::Seconds(20));
    EXPECT_EQ(visit.url_row.url(), "https://github.com/");
    EXPECT_EQ(visit.context_annotations.page_end_reason, 5);

    // TODO(tommycli): Add back visit.referring_visit_id() check after updating
    //  the HistoryService test methods to support that field.
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

  base::CancelableTaskTracker task_tracker_;

  // Used to verify the async callback is invoked.
  base::RunLoop run_loop_;
  base::RepeatingClosure run_loop_quit_;

  // Tracks the next available navigation ID to be associated with visits.
  int64_t next_navigation_id_ = 0;
};

class HistoryClustersServiceTest : public HistoryClustersServiceTestBase {
 public:
  HistoryClustersServiceTest() {
    scoped_feature_list_.InitAndEnableFeature(internal::kJourneys);
  }
};

TEST_F(HistoryClustersServiceTest, HardCapOnVisitsFetchedFromHistory) {
  Config config;
  config.is_journeys_enabled_no_locale_check = true;
  config.max_visits_to_cluster = 20;
  SetConfigForTesting(config);

  history::ContextID context_id = reinterpret_cast<history::ContextID>(1);
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

  history_clusters_service_->QueryClusters(
      ClusteringRequestSource::kKeywordCacheGeneration,
      /*begin_time=*/base::Time(), /*end_time=*/base::Time::Now(),
      base::DoNothing(),  // Only need to verify the correct request is sent.
      &task_tracker_);

  test_clustering_backend_->WaitForGetClustersCall();
  history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());

  EXPECT_EQ(test_clustering_backend_->LastClusteredVisits().size(), 20U);
}

TEST_F(HistoryClustersServiceTest, QueryClustersIncompleteAndPersistedVisits) {
  // Create 5 persisted visits with visit times 2, 1, 1, 60, and 1 days ago.
  AddHardcodedTestDataToHistoryService();

  auto days_ago = [](int days) { return base::Time::Now() - base::Days(days); };

  // Create incomplete visits; only 3 & 4 should be returned by the query.
  AddIncompleteVisit(6, 6, days_ago(1));
  AddIncompleteVisit(0, 0, days_ago(1));  // Missing history rows.
  AddIncompleteVisit(7, 7, days_ago(90));
  AddIncompleteVisit(8, 8, days_ago(0));   // Too recent.
  AddIncompleteVisit(9, 9, days_ago(93));  // Too old.
  AddIncompleteVisit(3, 3, days_ago(90));  // Visit 3 was added to the history
                                           // database with source synced.
  AddIncompleteVisit(
      10, 10, days_ago(1),
      ui::PageTransitionFromInt(805306372));  // Non-visible page transition.

  // Helper to repeatedly call `QueryClusters()`, with the continuation time
  // returned from the previous call, and return the visits sent to the
  // clustering backend.
  base::Time continuation_end_time = base::Time::Now();
  const auto next_query_clusters = [&]() {
    history_clusters_service_->QueryClusters(
        ClusteringRequestSource::kJourneysPage,
        /*begin_time=*/base::Time(), continuation_end_time,
        base::BindLambdaForTesting([&](std::vector<history::Cluster> clusters,
                                       base::Time continuation_end_time_temp) {
          continuation_end_time = continuation_end_time_temp;
        }),
        &task_tracker_);

    test_clustering_backend_->WaitForGetClustersCall();
    history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());

    // Persisted visits are ordered before incomplete visits. Persisted visits
    // are ordered newest first. Incomplete visits are ordered the same as they
    // were sent to the `HistoryClustersService`.
    test_clustering_backend_->FulfillCallback({});
    return test_clustering_backend_->LastClusteredVisits();
  };

  // 1st query should return visits 2, 5, & 6, the good, 1-day-old visits.
  // Visits 3, 0, and 10, also 1-day-old, are excluded since they're synced,
  // missing history rows, and non-visible transition respectively.
  auto visits = next_query_clusters();
  ASSERT_EQ(visits.size(), 3u);
  EXPECT_EQ(visits[0].visit_row.visit_id, 5);
  EXPECT_EQ(visits[1].visit_row.visit_id, 2);
  EXPECT_EQ(visits[2].visit_row.visit_id, 6);

  // 2nd query should return visit 1, a 2-day-old complete visit.
  visits = next_query_clusters();
  ASSERT_EQ(visits.size(), 1u);
  EXPECT_EQ(visits[0].visit_row.visit_id, 1);

  // 3rd query should return visit 4, a 30-day-old complete visit, since there
  // are no 3-to-29-day-old visits.
  visits = next_query_clusters();
  ASSERT_EQ(visits.size(), 1u);
  EXPECT_EQ(visits[0].visit_row.visit_id, 4);

  // 4th query should return visit 7, a 90-day-old incomplete visit, since there
  // are no 31-to-89-day-old visits.
  visits = next_query_clusters();
  ASSERT_EQ(visits.size(), 1u);
  EXPECT_EQ(visits[0].visit_row.visit_id, 7);
}

TEST_F(HistoryClustersServiceTest, EndToEndWithBackend) {
  base::HistogramTester histogram_tester;
  AddHardcodedTestDataToHistoryService();

  base::RunLoop run_loop;
  auto run_loop_quit = run_loop.QuitClosure();

  history_clusters_service_->QueryClusters(
      ClusteringRequestSource::kJourneysPage,
      /*begin_time=*/base::Time(),
      /*end_time=*/base::Time(),
      // This "expect" block is not run until after the fake response is sent
      // further down in this method.
      base::BindLambdaForTesting(
          [&](std::vector<history::Cluster> clusters, base::Time) {
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

            ASSERT_EQ(cluster.keywords.size(), 2u);
            EXPECT_EQ(cluster.keywords[0], u"apples");
            EXPECT_EQ(cluster.keywords[1], u"Red Oranges");

            cluster = clusters[1];
            visits = cluster.visits;
            ASSERT_EQ(visits.size(), 1u);
            EXPECT_EQ(visits[0].annotated_visit.url_row.url(),
                      "https://github.com/");
            EXPECT_EQ(visits[0].annotated_visit.visit_row.visit_time,
                      GetHardcodedTestVisits()[1].visit_row.visit_time);
            EXPECT_EQ(visits[0].annotated_visit.url_row.title(),
                      u"Code Storage Title");
            EXPECT_TRUE(cluster.keywords.empty());

            run_loop_quit.Run();
          }),
      &task_tracker_);

  AwaitAndVerifyTestClusteringBackendRequest();

  std::vector<history::Cluster> clusters;
  clusters.push_back(
      history::Cluster(0,
                       {
                           test_clustering_backend_->GetVisitById(2),
                           test_clustering_backend_->GetVisitById(5),
                       },
                       {u"apples", u"Red Oranges"},
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
      "History.Clusters.Backend.NumVisitsToCluster", 2, 1);
  histogram_tester.ExpectTotalCount(
      "History.Clusters.Backend.GetClustersLatency", 1);
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

TEST_F(HistoryClustersServiceTest, DoesQueryMatchAnyCluster) {
  AddHardcodedTestDataToHistoryService();

  // Verify that initially, the test keyword doesn't match anything, but this
  // query should have kicked off a cache population request.
  EXPECT_FALSE(history_clusters_service_->DoesQueryMatchAnyCluster("apples"));

  // Helper to flush out the multiple history and cluster backend requests made
  // by `DoesQueryMatchAnyCluster()`. It won't populate the cache until all its
  // requests have been completed. It makes 1 request (to each) per unique day
  // with at least 1 visit; i.e. `number_of_days_with_visits`.
  const auto flush_keyword_requests = [&](size_t number_of_days_with_visits) {
    test_clustering_backend_->WaitForGetClustersCall();

    std::vector<history::Cluster> clusters;
    clusters.push_back(
        history::Cluster(0,
                         {
                             test_clustering_backend_->GetVisitById(5),
                             test_clustering_backend_->GetVisitById(2),
                         },
                         {u"apples", u"oranges", u"z", u"apples bananas"},
                         /*should_show_on_prominent_ui_surfaces=*/true));
    clusters.push_back(
        history::Cluster(0,
                         {
                             test_clustering_backend_->GetVisitById(5),
                             test_clustering_backend_->GetVisitById(2),
                         },
                         {u"sensitive"},
                         /*should_show_on_prominent_ui_surfaces=*/false));
    clusters.push_back(
        history::Cluster(0,
                         {
                             test_clustering_backend_->GetVisitById(5),
                         },
                         {u"singlevisit"},
                         /*should_show_on_prominent_ui_surfaces=*/true));

    test_clustering_backend_->FulfillCallback(clusters);

    // `DoesQueryMatchAnyCluster()` will continue making history and cluster
    // backend requests until it has exhausted history. We have to flush out
    // these requests before it will populate the cache.
    for (size_t i = 0; i < number_of_days_with_visits - 1; ++i) {
      test_clustering_backend_->WaitForGetClustersCall();
      history::BlockUntilHistoryProcessesPendingRequests(
          history_service_.get());
      test_clustering_backend_->FulfillCallback({});
    }
    // One last wait to flush out the last, empty history request.
    history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());
  };

  // Hardcoded test visits span 3 days (1-day-old, 2-days-old, and 60-day-old).
  flush_keyword_requests(3);

  // Now the exact query should match the populated cache.
  EXPECT_TRUE(history_clusters_service_->DoesQueryMatchAnyCluster("apples"));

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
  flush_keyword_requests(2);

  // The keyword cache should be repopulated.
  EXPECT_TRUE(history_clusters_service_->DoesQueryMatchAnyCluster("apples"));
}

TEST_F(HistoryClustersServiceTest, DoesQueryMatchAnyClusterSecondaryCache) {
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
  ASSERT_EQ(visits.size(), 2u);
  EXPECT_EQ(visits[0].visit_row.visit_id, 1);
  EXPECT_EQ(visits[1].visit_row.visit_id, 2);

  // Send the cluster response and verify the keyword was cached.
  std::vector<history::Cluster> clusters2;
  clusters2.push_back(
      history::Cluster(0,
                       {
                           test_clustering_backend_->GetVisitById(1),
                           test_clustering_backend_->GetVisitById(2),
                       },
                       {u"peach", u""},
                       /*should_show_on_prominent_ui_surfaces=*/true));
  test_clustering_backend_->FulfillCallback(clusters2);
  history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());
  EXPECT_TRUE(history_clusters_service_->DoesQueryMatchAnyCluster("peach"));
}

TEST_F(HistoryClustersServiceTest, DoesURLMatchAnyClusterWithNoisyURLs) {
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

  // Helper to flush out the multiple history and cluster backend requests made
  // by `DoesURLMatchAnyCluster()`. It won't populate the cache until all its
  // requests have been completed. It makes 1 request (to each) per unique day
  // with at least 1 visit; i.e. `number_of_days_with_visits`.
  const auto flush_keyword_requests = [&](size_t number_of_days_with_visits) {
    test_clustering_backend_->WaitForGetClustersCall();

    std::vector<history::Cluster> clusters;
    clusters.push_back(history::Cluster(
        0,
        {
            test_clustering_backend_->GetVisitById(5),
            test_clustering_backend_->GetVisitById(
                /*visit_id=*/2, /*score=*/0.0, /*engagement_score=*/20.0),
        },
        {u"apples", u"oranges", u"z", u"apples bananas"},
        /*should_show_on_prominent_ui_surfaces=*/true));
    clusters.push_back(
        history::Cluster(0,
                         {
                             test_clustering_backend_->GetVisitById(5),
                             test_clustering_backend_->GetVisitById(2),
                         },
                         {u"sensitive"},
                         /*should_show_on_prominent_ui_surfaces=*/false));
    clusters.push_back(
        history::Cluster(0,
                         {
                             test_clustering_backend_->GetVisitById(2),
                         },
                         {u"singlevisit"},
                         /*should_show_on_prominent_ui_surfaces=*/true));

    test_clustering_backend_->FulfillCallback(clusters);

    // `DoesQueryMatchAnyCluster()` will continue making history and cluster
    // backend requests until it has exhausted history. We have to flush out
    // these requests before it will populate the cache.
    for (size_t i = 0; i < number_of_days_with_visits - 1; ++i) {
      test_clustering_backend_->WaitForGetClustersCall();
      history::BlockUntilHistoryProcessesPendingRequests(
          history_service_.get());
      test_clustering_backend_->FulfillCallback({});
    }
    // One last wait to flush out the last, empty history request.
    history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());
  };

  // Hardcoded test visits span 3 days (1-day-old, 2-days-old, and 60-day-old).
  flush_keyword_requests(3);

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
  flush_keyword_requests(2);

  // The keyword cache should be repopulated.
  EXPECT_TRUE(history_clusters_service_->DoesURLMatchAnyCluster(
      ComputeURLKeywordForLookup(GURL("https://second-1-day-old-visit.com/"))));
}

TEST_F(HistoryClustersServiceTest, DoesURLMatchAnyClusterNoNoisyURLs) {
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

  // Helper to flush out the multiple history and cluster backend requests made
  // by `DoesURLMatchAnyCluster()`. It won't populate the cache until all its
  // requests have been completed. It makes 1 request (to each) per unique day
  // with at least 1 visit; i.e. `number_of_days_with_visits`.
  const auto flush_keyword_requests = [&](size_t number_of_days_with_visits) {
    test_clustering_backend_->WaitForGetClustersCall();

    std::vector<history::Cluster> clusters;
    clusters.push_back(history::Cluster(
        0,
        {
            test_clustering_backend_->GetVisitById(5),
            test_clustering_backend_->GetVisitById(
                /*visit_id=*/2, /*score=*/0.0, /*engagement_score=*/20.0),
        },
        {u"apples", u"oranges", u"z", u"apples bananas"},
        /*should_show_on_prominent_ui_surfaces=*/true));
    clusters.push_back(
        history::Cluster(0,
                         {
                             test_clustering_backend_->GetVisitById(5),
                             test_clustering_backend_->GetVisitById(2),
                         },
                         {u"sensitive"},
                         /*should_show_on_prominent_ui_surfaces=*/false));
    clusters.push_back(
        history::Cluster(0,
                         {
                             test_clustering_backend_->GetVisitById(2),
                         },
                         {u"singlevisit"},
                         /*should_show_on_prominent_ui_surfaces=*/true));

    test_clustering_backend_->FulfillCallback(clusters);

    // `DoesQueryMatchAnyCluster()` will continue making history and cluster
    // backend requests until it has exhausted history. We have to flush out
    // these requests before it will populate the cache.
    for (size_t i = 0; i < number_of_days_with_visits - 1; ++i) {
      test_clustering_backend_->WaitForGetClustersCall();
      history::BlockUntilHistoryProcessesPendingRequests(
          history_service_.get());
      test_clustering_backend_->FulfillCallback({});
    }
    // One last wait to flush out the last, empty history request.
    history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());
  };

  // Hardcoded test visits span 3 days (1-day-old, 2-days-old, and 60-day-old).
  flush_keyword_requests(3);

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
  flush_keyword_requests(2);

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

  // Kick off cluster request.
  EXPECT_FALSE(history_clusters_service_->DoesQueryMatchAnyCluster("peach"));
  test_clustering_backend_->WaitForGetClustersCall();
  ASSERT_EQ(test_clustering_backend_->LastClusteredVisits().size(), 7u);

  // Create 4 clusters:
  std::vector<history::AnnotatedVisit> visits =
      test_clustering_backend_->LastClusteredVisits();
  std::vector<history::Cluster> clusters;
  // 1) A cluster with 4 phrases and 6 words. The next cluster's keywords should
  // also be cached since we have less than 5 phrases.
  clusters.push_back(
      history::Cluster(0,
                       {
                           test_clustering_backend_->GetVisitById(1),
                           test_clustering_backend_->GetVisitById(2),
                       },
                       {u"one", u"two", u"three", u"four five six"},
                       /*should_show_on_prominent_ui_surfaces=*/true));
  // 2) The 2nd cluster has only 1 visit. Since it's keywords won't be cached,
  // they should not affect the max.
  clusters.push_back(
      history::Cluster(0,
                       {
                           test_clustering_backend_->GetVisitById(3),
                       },
                       {u"ignored not cached", u"elephant penguin kangaroo"},
                       /*should_show_on_prominent_ui_surfaces=*/true));
  // 3) With this 3rd cluster, we'll have 5 phrases and 7 words. Now that we've
  // reached 5 phrases, the next cluster's keywords should not be cached.
  clusters.push_back(
      history::Cluster(0,
                       {
                           test_clustering_backend_->GetVisitById(4),
                           test_clustering_backend_->GetVisitById(5),
                       },
                       {u"seven"},
                       /*should_show_on_prominent_ui_surfaces=*/true));
  // 4) The 4th cluster's keywords should not be cached since we've reached 5
  // phrases.
  clusters.push_back(
      history::Cluster(0,
                       {
                           test_clustering_backend_->GetVisitById(6),
                           test_clustering_backend_->GetVisitById(7),
                       },
                       {u"eight"},
                       /*should_show_on_prominent_ui_surfaces=*/true));
  test_clustering_backend_->FulfillCallback(clusters);
  history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());

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
