// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/context_clusterer_history_service_observer.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/history/core/browser/history_service.h"
#include "components/history_clusters/core/config.h"
#include "components/optimization_guide/core/new_optimization_guide_decider.h"
#include "components/search_engines/template_url_service.h"
#include "components/site_engagement/core/site_engagement_score_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_clusters {

namespace {

using ::testing::_;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SaveArg;

history::URLRows CreateURLRows(const std::vector<GURL>& urls) {
  history::URLRows url_rows;
  for (const auto& url : urls) {
    url_rows.emplace_back(history::URLRow(url));
  }
  return url_rows;
}

class TestOptimizationGuideDecider
    : public optimization_guide::NewOptimizationGuideDecider {
 public:
  TestOptimizationGuideDecider() = default;
  ~TestOptimizationGuideDecider() override = default;

  void RegisterOptimizationTypes(
      const std::vector<optimization_guide::proto::OptimizationType>&
          optimization_types) override {
    ASSERT_EQ(optimization_types.size(), 1u);
    ASSERT_EQ(optimization_guide::proto::HISTORY_CLUSTERS,
              optimization_types[0]);
  }

  void CanApplyOptimization(
      const GURL& url,
      optimization_guide::proto::OptimizationType optimization_type,
      optimization_guide::OptimizationGuideDecisionCallback callback) override {
    NOTREACHED();
  }

  optimization_guide::OptimizationGuideDecision CanApplyOptimization(
      const GURL& url,
      optimization_guide::proto::OptimizationType optimization_type,
      optimization_guide::OptimizationMetadata* optimization_metadata)
      override {
    DCHECK_EQ(optimization_guide::proto::HISTORY_CLUSTERS, optimization_type);
    return url.host() == "shouldskip.com"
               ? optimization_guide::OptimizationGuideDecision::kFalse
               : optimization_guide::OptimizationGuideDecision::kTrue;
  }

  void CanApplyOptimizationOnDemand(
      const std::vector<GURL>& urls,
      const base::flat_set<optimization_guide::proto::OptimizationType>&
          optimization_types,
      optimization_guide::proto::RequestContext request_context,
      optimization_guide::OnDemandOptimizationGuideDecisionRepeatingCallback
          callback) override {}
};

const TemplateURLService::Initializer kTemplateURLData[] = {
    {"default-engine.com", "http://default-engine.com/search?q={searchTerms}",
     "Default"},
    {"non-default-engine.com", "http://non-default-engine.com?q={searchTerms}",
     "Not Default"},
};

class MockHistoryService : public history::HistoryService {
 public:
  MockHistoryService() = default;
  ~MockHistoryService() override = default;

  MOCK_METHOD(base::CancelableTaskTracker::TaskId,
              ReserveNextClusterId,
              (ClusterIdCallback callback,
               base::CancelableTaskTracker* tracker),
              (override));
  MOCK_METHOD(base::CancelableTaskTracker::TaskId,
              AddVisitsToCluster,
              (int64_t,
               const std::vector<history::ClusterVisit>&,
               base::OnceClosure callback,
               base::CancelableTaskTracker*),
              (override));
  MOCK_METHOD(base::CancelableTaskTracker::TaskId,
              UpdateClusterVisit,
              (const history::ClusterVisit&,
               base::OnceClosure callback,
               base::CancelableTaskTracker*),
              (override));

  base::CancelableTaskTracker::TaskId CaptureClusterIdCallback(
      ClusterIdCallback callback,
      base::CancelableTaskTracker* tracker) {
    cluster_id_callback_ = std::move(callback);
    return base::CancelableTaskTracker::TaskId();
  }

  // Runs the last cluster id callback received with `cluster_id`.
  void RunLastClusterIdCallbackWithClusterId(int64_t cluster_id) {
    std::move(cluster_id_callback_).Run(cluster_id);
  }

  base::CancelableTaskTracker::TaskId RunAddVisitsToClusterCallback(
      int64_t cluster_id,
      const std::vector<history::ClusterVisit>& cluster_visits,
      base::OnceClosure callback,
      base::CancelableTaskTracker* tracker) {
    std::move(callback).Run();
    return base::CancelableTaskTracker::TaskId();
  }

  base::CancelableTaskTracker::TaskId RunUpdateClusterVisitCallback(
      const history::ClusterVisit& cluster_visit,
      base::OnceClosure callback,
      base::CancelableTaskTracker* tracker) {
    std::move(callback).Run();
    return base::CancelableTaskTracker::TaskId();
  }

 private:
  ClusterIdCallback cluster_id_callback_;
};

class TestSiteEngagementScoreProvider
    : public site_engagement::SiteEngagementScoreProvider {
 public:
  TestSiteEngagementScoreProvider() = default;
  ~TestSiteEngagementScoreProvider() override = default;

  double GetScore(const GURL& url) const override {
    ++count_get_score_invocations_;
    return static_cast<double>(count_get_score_invocations_);
  }

  double GetTotalEngagementPoints() const override { return 1; }

  size_t num_get_score_invocations() const {
    return count_get_score_invocations_;
  }

 private:
  mutable size_t count_get_score_invocations_ = 0;
};

// Gets the visit IDs in `visits`.
std::vector<history::VisitID> GetClusterVisitIds(
    const std::vector<history::ClusterVisit>& visits) {
  std::vector<history::VisitID> visit_ids;
  visit_ids.reserve(visits.size());
  for (const auto& visit : visits) {
    visit_ids.push_back(visit.annotated_visit.visit_row.visit_id);
  }
  return visit_ids;
}

}  // namespace

class ContextClustererHistoryServiceObserverTest : public testing::Test {
 public:
  ContextClustererHistoryServiceObserverTest() = default;
  ~ContextClustererHistoryServiceObserverTest() override = default;

  void SetUp() override {
    history_service_ =
        std::make_unique<testing::StrictMock<MockHistoryService>>();

    template_url_service_ = std::make_unique<TemplateURLService>(
        kTemplateURLData, std::size(kTemplateURLData));

    optimization_guide_decider_ =
        std::make_unique<TestOptimizationGuideDecider>();

    engagement_score_provider_ =
        std::make_unique<TestSiteEngagementScoreProvider>();

    // Instantiate observer.
    observer_ = std::make_unique<ContextClustererHistoryServiceObserver>(
        history_service_.get(), template_url_service_.get(),
        optimization_guide_decider_.get(), engagement_score_provider_.get());
    observer_->OverrideClockForTesting(task_environment_.GetMockClock());
  }

  void TearDown() override {
    // Just reset to original config at end of each test.
    Config config;
    SetConfigForTesting(config);
  }

  // Sets the config so that we expect to persist clusters and visits using this
  // code path.
  void SetPersistenceExpectedConfig() {
    Config config;
    config.persist_clusters_in_history_db = true;
    config.use_navigation_context_clusters = true;
    SetConfigForTesting(config);
  }

  // Simulates a visit to URL.
  void VisitURL(const GURL& url,
                history::VisitID visit_id,
                base::Time visit_time,
                history::VisitID opener_visit = history::kInvalidVisitID,
                history::VisitID referring_visit = history::kInvalidVisitID,
                bool is_synced_visit = false,
                bool is_visible_visit = true) {
    history::URLRow url_row(url);
    history::VisitRow new_visit;
    new_visit.visit_id = visit_id;
    new_visit.visit_time = visit_time;
    new_visit.opener_visit = opener_visit;
    new_visit.referring_visit = referring_visit;
    new_visit.originator_cache_guid = is_synced_visit ? "otherdevice" : "";
    new_visit.transition = ui::PageTransitionFromInt(
        (is_visible_visit ? ui::PAGE_TRANSITION_LINK
                          : ui::PAGE_TRANSITION_AUTO_SUBFRAME) |
        ui::PAGE_TRANSITION_CHAIN_END);
    observer_->OnURLVisited(history_service_.get(), url_row, new_visit);
  }

  // Simulates deleting `urls` from history. If `urls` is empty, we will
  // simulate deleting all history.
  void DeleteHistory(const std::vector<GURL>& urls) {
    history::DeletionInfo deletion_info =
        urls.empty() ? history::DeletionInfo::ForAllHistory()
                     : history::DeletionInfo::ForUrls(CreateURLRows(urls),
                                                      /*favicon_urls=*/{});
    observer_->OnURLsDeleted(history_service_.get(), deletion_info);
  }

  // Move clock forward by `time_delta`.
  void MoveClockForwardBy(base::TimeDelta time_delta) {
    task_environment_.FastForwardBy(time_delta);
    task_environment_.RunUntilIdle();
  }

  // Returns the number of clusters created by |observer_|.
  int64_t GetNumClustersCreated() const {
    return observer_->num_clusters_created();
  }

  // Returns the current time of this task environment's mock clock.
  base::Time Now() { return task_environment_.GetMockClock()->Now(); }

  // Returns the number of times engagement score provider has been invoked.
  size_t num_get_score_invocations() const {
    return engagement_score_provider_
               ? engagement_score_provider_->num_get_score_invocations()
               : 0;
  }

 protected:
  std::unique_ptr<MockHistoryService> history_service_;

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<TemplateURLService> template_url_service_;
  std::unique_ptr<TestOptimizationGuideDecider> optimization_guide_decider_;
  std::unique_ptr<TestSiteEngagementScoreProvider> engagement_score_provider_;
  std::unique_ptr<ContextClustererHistoryServiceObserver> observer_;
};

TEST_F(ContextClustererHistoryServiceObserverTest, ClusterOneVisit) {
  base::HistogramTester histogram_tester;

  SetPersistenceExpectedConfig();
  int64_t cluster_id = 123;

  EXPECT_CALL(*history_service_,
              ReserveNextClusterId(base::test::IsNotNullCallback(), _))
      .WillOnce(Invoke(history_service_.get(),
                       &MockHistoryService::CaptureClusterIdCallback));
  VisitURL(GURL("https://example.com"), 1, base::Time::FromTimeT(123));

  std::vector<history::ClusterVisit> got_cluster_visits;
  EXPECT_CALL(
      *history_service_,
      AddVisitsToCluster(cluster_id, _, base::test::IsNotNullCallback(), _))
      .WillOnce(
          DoAll(SaveArg<1>(&got_cluster_visits),
                Invoke(history_service_.get(),
                       &MockHistoryService::RunAddVisitsToClusterCallback)));
  history_service_->RunLastClusterIdCallbackWithClusterId(cluster_id);

  EXPECT_EQ(1, GetNumClustersCreated());
  EXPECT_THAT(GetClusterVisitIds(got_cluster_visits), ElementsAre(1));

  histogram_tester.ExpectTotalCount(
      "History.Clusters.ContextClusterer.DbLatency.ReserveNextClusterId", 1);
  histogram_tester.ExpectTotalCount(
      "History.Clusters.ContextClusterer.DbLatency.AddVisitsToCluster", 1);
}

TEST_F(ContextClustererHistoryServiceObserverTest,
       ClusterTwoVisitsTiedByReferringVisit) {
  base::HistogramTester histogram_tester;

  SetPersistenceExpectedConfig();
  int64_t cluster_id = 123;

  EXPECT_CALL(*history_service_,
              ReserveNextClusterId(base::test::IsNotNullCallback(), _))
      .WillOnce(Invoke(history_service_.get(),
                       &MockHistoryService::CaptureClusterIdCallback));
  VisitURL(GURL("https://example.com"), 1, base::Time::FromTimeT(123));
  VisitURL(GURL("https://example.com/2"), 2, base::Time::FromTimeT(123),
           /*opener_visit=*/history::kInvalidVisitID, /*referring_visit=*/1);

  // Should persist all visits for the cluster when callback is run.
  std::vector<history::ClusterVisit> got_cluster_visits;
  EXPECT_CALL(
      *history_service_,
      AddVisitsToCluster(cluster_id, _, base::test::IsNotNullCallback(), _))
      .WillOnce(
          DoAll(SaveArg<1>(&got_cluster_visits),
                Invoke(history_service_.get(),
                       &MockHistoryService::RunAddVisitsToClusterCallback)));
  history_service_->RunLastClusterIdCallbackWithClusterId(cluster_id);

  EXPECT_EQ(1, GetNumClustersCreated());
  EXPECT_THAT(GetClusterVisitIds(got_cluster_visits), ElementsAre(1, 2));

  // Add a visit that is not visible to the user but refers to one of the visits
  // in the cluster. Should not be persisted.
  VisitURL(GURL("https://example.com/notvisible"), 3,
           base::Time::FromTimeT(124), 1, history::kInvalidVisitID, false,
           false);

  histogram_tester.ExpectTotalCount(
      "History.Clusters.ContextClusterer.DbLatency.ReserveNextClusterId", 1);
  histogram_tester.ExpectTotalCount(
      "History.Clusters.ContextClusterer.DbLatency.AddVisitsToCluster", 1);
}

TEST_F(ContextClustererHistoryServiceObserverTest,
       ClusterTwoVisitsTiedByOpenerVisit) {
  base::HistogramTester histogram_tester;

  SetPersistenceExpectedConfig();
  int64_t cluster_id = 123;

  EXPECT_CALL(*history_service_,
              ReserveNextClusterId(base::test::IsNotNullCallback(), _))
      .WillOnce(Invoke(history_service_.get(),
                       &MockHistoryService::CaptureClusterIdCallback));
  VisitURL(GURL("https://example.com"), 1, base::Time::FromTimeT(123));

  // Should persist all visits for the cluster when callback is run.
  std::vector<history::ClusterVisit> got_first_cluster_visits;
  EXPECT_CALL(
      *history_service_,
      AddVisitsToCluster(cluster_id, _, base::test::IsNotNullCallback(), _))
      .WillOnce(
          DoAll(SaveArg<1>(&got_first_cluster_visits),
                Invoke(history_service_.get(),
                       &MockHistoryService::RunAddVisitsToClusterCallback)));
  history_service_->RunLastClusterIdCallbackWithClusterId(cluster_id);

  // Should persist as is since we already have the persisted cluster id at this
  // visit.
  std::vector<history::ClusterVisit> got_second_cluster_visits;
  EXPECT_CALL(
      *history_service_,
      AddVisitsToCluster(cluster_id, _, base::test::IsNotNullCallback(), _))
      .WillOnce(
          DoAll(SaveArg<1>(&got_second_cluster_visits),
                Invoke(history_service_.get(),
                       &MockHistoryService::RunAddVisitsToClusterCallback)));
  VisitURL(GURL("https://example.com/2"), 2, base::Time::FromTimeT(123),
           /*opener_visit=*/1, /*referring_visit=*/history::kInvalidVisitID);

  EXPECT_EQ(1, GetNumClustersCreated());
  EXPECT_THAT(GetClusterVisitIds(got_first_cluster_visits), ElementsAre(1));
  EXPECT_THAT(GetClusterVisitIds(got_second_cluster_visits), ElementsAre(2));

  histogram_tester.ExpectTotalCount(
      "History.Clusters.ContextClusterer.DbLatency.ReserveNextClusterId", 1);
  histogram_tester.ExpectTotalCount(
      "History.Clusters.ContextClusterer.DbLatency.AddVisitsToCluster", 2);
}

TEST_F(ContextClustererHistoryServiceObserverTest,
       ClusterTwoVisitsOpenerTakesPrecedenceOverReferrer) {
  VisitURL(GURL("https://example.com"), 1, base::Time::FromTimeT(123));
  VisitURL(GURL("https://hasopenerbutbadreferrer.com"), 2,
           base::Time::FromTimeT(123),
           /*opener_visit=*/1, /*referring_visit=*/6);
  VisitURL(GURL("https://hasbadopenerbutgoodreferrer.com"), 3,
           base::Time::FromTimeT(123),
           /*opener_visit=*/6, /*referring_visit=*/2);

  EXPECT_EQ(1, GetNumClustersCreated());
}

TEST_F(ContextClustererHistoryServiceObserverTest, ClusterTwoVisitsTiedByURL) {
  VisitURL(GURL("https://example.com"), 1, base::Time::FromTimeT(123));
  VisitURL(GURL("https://example.com"), 2, base::Time::FromTimeT(123),
           /*opener_visit=*/history::kInvalidVisitID,
           /*referring_visit=*/history::kInvalidVisitID);

  EXPECT_EQ(1, GetNumClustersCreated());
}

TEST_F(ContextClustererHistoryServiceObserverTest,
       ClusterVisitsTiedBySearchURL) {
  VisitURL(GURL("http://default-engine.com/search?q=foo"), 1,
           base::Time::FromTimeT(123));
  VisitURL(GURL("http://default-engine.com/search?q=foo#whatever"), 2,
           base::Time::FromTimeT(123),
           /*opener_visit=*/history::kInvalidVisitID,
           /*referring_visit=*/history::kInvalidVisitID);
  // Simulate a back-forward navigation.
  VisitURL(GURL("http://default-engine.com/search?q=foo"), 3,
           base::Time::FromTimeT(124));

  EXPECT_EQ(1, GetNumClustersCreated());
}

TEST_F(ContextClustererHistoryServiceObserverTest, SplitClusterOnSearchTerm) {
  base::HistogramTester histogram_tester;

  SetPersistenceExpectedConfig();

  EXPECT_CALL(*history_service_,
              ReserveNextClusterId(base::test::IsNotNullCallback(), _))
      .WillOnce(Invoke(history_service_.get(),
                       &MockHistoryService::CaptureClusterIdCallback));
  VisitURL(GURL("http://default-engine.com/search?q=foo"), 1,
           base::Time::FromTimeT(123));

  int64_t cluster_id = 123;
  std::vector<history::ClusterVisit> got_first_cluster_visits;
  EXPECT_CALL(
      *history_service_,
      AddVisitsToCluster(cluster_id, _, base::test::IsNotNullCallback(), _))
      .WillOnce(
          DoAll(SaveArg<1>(&got_first_cluster_visits),
                Invoke(history_service_.get(),
                       &MockHistoryService::RunAddVisitsToClusterCallback)));
  history_service_->RunLastClusterIdCallbackWithClusterId(cluster_id);

  EXPECT_CALL(*history_service_,
              ReserveNextClusterId(base::test::IsNotNullCallback(), _))
      .WillOnce(Invoke(history_service_.get(),
                       &MockHistoryService::CaptureClusterIdCallback));
  VisitURL(GURL("http://default-engine.com/search?q=otherterm"), 2,
           base::Time::FromTimeT(123),
           /*opener_visit=*/1);

  std::vector<history::ClusterVisit> got_second_cluster_visits;
  cluster_id = 124;
  EXPECT_CALL(
      *history_service_,
      AddVisitsToCluster(cluster_id, _, base::test::IsNotNullCallback(), _))
      .WillOnce(
          DoAll(SaveArg<1>(&got_second_cluster_visits),
                Invoke(history_service_.get(),
                       &MockHistoryService::RunAddVisitsToClusterCallback)));
  history_service_->RunLastClusterIdCallbackWithClusterId(cluster_id);

  EXPECT_EQ(2, GetNumClustersCreated());
  EXPECT_THAT(GetClusterVisitIds(got_first_cluster_visits), ElementsAre(1));
  EXPECT_THAT(GetClusterVisitIds(got_second_cluster_visits), ElementsAre(2));

  histogram_tester.ExpectTotalCount(
      "History.Clusters.ContextClusterer.DbLatency.ReserveNextClusterId", 2);
  histogram_tester.ExpectTotalCount(
      "History.Clusters.ContextClusterer.DbLatency.AddVisitsToCluster", 2);
}

TEST_F(ContextClustererHistoryServiceObserverTest,
       SplitClusterOnNavigationTime) {
  VisitURL(GURL("https://example.com"), 1, base::Time::FromTimeT(123));
  VisitURL(GURL("https://example.com/2"), 2,
           base::Time::FromTimeT(123) + base::Milliseconds(1) +
               GetConfig().cluster_navigation_time_cutoff,
           /*opener_visit=*/1, /*referring_visit=*/history::kInvalidVisitID);

  EXPECT_EQ(2, GetNumClustersCreated());
}

TEST_F(ContextClustererHistoryServiceObserverTest,
       DoesNotClusterSyncedVisitsButUpdatesDetails) {
  base::HistogramTester histogram_tester;

  SetPersistenceExpectedConfig();

  history::ClusterVisit updated_cluster_visit;
  EXPECT_CALL(*history_service_,
              UpdateClusterVisit(_, base::test::IsNotNullCallback(), _))
      .WillOnce(
          DoAll(SaveArg<0>(&updated_cluster_visit),
                Invoke(history_service_.get(),
                       &MockHistoryService::RunUpdateClusterVisitCallback)));

  VisitURL(GURL("https://example.com"), 1, base::Time::FromTimeT(123),
           history::kInvalidVisitID, history::kInvalidVisitID,
           /*is_synced_visit=*/true);

  EXPECT_EQ(0, GetNumClustersCreated());
  // Details should be somewhat populated.
  EXPECT_FALSE(updated_cluster_visit.normalized_url.is_empty());
  EXPECT_FALSE(updated_cluster_visit.url_for_deduping.is_empty());
  EXPECT_FALSE(updated_cluster_visit.url_for_display.empty());
  EXPECT_GT(updated_cluster_visit.engagement_score, 0);
  EXPECT_EQ(1u, num_get_score_invocations());

  histogram_tester.ExpectTotalCount(
      "History.Clusters.ContextClusterer.VisitProcessingLatency.UrlVisited", 1);
  histogram_tester.ExpectTotalCount(
      "History.Clusters.ContextClusterer.DbLatency.UpdateClusterVisit", 1);

  // Visit the same host. We only expect for the engagement score provider to be
  // called once.
  EXPECT_CALL(*history_service_,
              UpdateClusterVisit(_, base::test::IsNotNullCallback(), _))
      .WillOnce(
          DoAll(SaveArg<0>(&updated_cluster_visit),
                Invoke(history_service_.get(),
                       &MockHistoryService::RunUpdateClusterVisitCallback)));

  VisitURL(GURL("https://example.com/123"), 1, base::Time::FromTimeT(123),
           history::kInvalidVisitID, history::kInvalidVisitID,
           /*is_synced_visit=*/true);

  EXPECT_EQ(1u, num_get_score_invocations());
}

TEST_F(ContextClustererHistoryServiceObserverTest,
       SearchNormalizedUrlIsNotAdjusted) {
  base::HistogramTester histogram_tester;

  SetPersistenceExpectedConfig();

  history::ClusterVisit updated_cluster_visit;
  EXPECT_CALL(*history_service_,
              UpdateClusterVisit(_, base::test::IsNotNullCallback(), _))
      .WillOnce(
          DoAll(SaveArg<0>(&updated_cluster_visit),
                Invoke(history_service_.get(),
                       &MockHistoryService::RunUpdateClusterVisitCallback)));

  VisitURL(GURL("http://default-engine.com/search?q=foo#abc"), 1,
           base::Time::FromTimeT(123), history::kInvalidVisitID,
           history::kInvalidVisitID,
           /*is_synced_visit=*/true);

  EXPECT_EQ(updated_cluster_visit.normalized_url,
            GURL("http://default-engine.com/search?q=foo"));
  EXPECT_EQ(updated_cluster_visit.url_for_deduping,
            GURL("http://default-engine.com/search?q=foo"));
}

TEST_F(ContextClustererHistoryServiceObserverTest, SkipsBlocklistedHost) {
  base::HistogramTester histogram_tester;

  SetPersistenceExpectedConfig();

  VisitURL(GURL("https://shouldskip.com"), 1, base::Time::FromTimeT(123));

  EXPECT_EQ(0, GetNumClustersCreated());

  // Visit processing histogram should still be recorded.
  histogram_tester.ExpectTotalCount(
      "History.Clusters.ContextClusterer.VisitProcessingLatency.UrlVisited", 1);

  histogram_tester.ExpectTotalCount(
      "History.Clusters.ContextClusterer.DbLatency.ReserveNextClusterId", 0);
}

TEST_F(ContextClustererHistoryServiceObserverTest, MultipleClusters) {
  VisitURL(GURL("https://example.com"), 1, base::Time::FromTimeT(1));
  VisitURL(GURL("https://example.com/2"), 2, base::Time::FromTimeT(2), 1);
  VisitURL(GURL("https://whatever.com"), 3, base::Time::FromTimeT(3));
  VisitURL(GURL("https://example.com"), 4, base::Time::FromTimeT(4));
  VisitURL(GURL("https://nonexistentreferrer.com"), 10,
           base::Time::FromTimeT(10), 6);

  // The clusters should be (1, 2, 4), (3), (10).
  EXPECT_EQ(3, GetNumClustersCreated());
}

TEST_F(ContextClustererHistoryServiceObserverTest, CleansUpClusters) {
  {
    base::HistogramTester histogram_tester;

    // Simulate a now time that is "old"-ish to make sure these get cleaned up
    // appropriately.
    base::Time now =
        Now() - GetConfig().cluster_navigation_time_cutoff - base::Minutes(1);
    VisitURL(GURL("https://example.com"), 1, now);
    VisitURL(GURL("https://example.com/2"), 2, now + base::Milliseconds(2), 1);
    VisitURL(GURL("https://whatever.com"), 3, now + base::Milliseconds(4));
    VisitURL(GURL("https://example.com"), 4, now + base::Milliseconds(10));
    // Make sure this last cluster doesn't get cleaned up, so use the actual
    // "Now" value.
    VisitURL(
        GURL("https://nonexistentreferrer.com"), 10,
        Now() + base::Minutes(
                    GetConfig().cluster_navigation_time_cutoff.InMinutes() / 2),
        6);
    // The clusters should be (1, 2, 4), (3), (10).
    EXPECT_EQ(3, GetNumClustersCreated());

    // Force a cleanup pass.
    MoveClockForwardBy(GetConfig().context_clustering_clean_up_duration);

    histogram_tester.ExpectUniqueSample(
        "History.Clusters.ContextClusterer.NumClusters.AtCleanUp", 3, 1);
    histogram_tester.ExpectUniqueSample(
        "History.Clusters.ContextClusterer.NumClusters.CleanedUp", 2, 1);
    // Should not finalize cluster with visit 10.
    histogram_tester.ExpectUniqueSample(
        "History.Clusters.ContextClusterer.NumClusters.PostCleanUp", 1, 1);

    histogram_tester.ExpectTotalCount(
        "History.Clusters.ContextClusterer.VisitProcessingLatency.UrlVisited",
        5);
    histogram_tester.ExpectTotalCount(
        "History.Clusters.ContextClusterer.VisitProcessingLatency.CleanUpTimer",
        1);
  }

  // This should create a new cluster.
  VisitURL(GURL("https://example.com"), 11, Now(), 4);
  // This should connect to the cluster with visit 10.
  VisitURL(GURL("https://newvisit.com"), 12, Now(), 10);

  // Expect only one more cluster to be created, which makes 4 total.
  EXPECT_EQ(4, GetNumClustersCreated());

  // Make sure everything is cleaned up eventually.
  {
    base::HistogramTester histogram_tester;

    MoveClockForwardBy(2 * GetConfig().cluster_navigation_time_cutoff);

    histogram_tester.ExpectBucketCount(
        "History.Clusters.ContextClusterer.NumClusters.PostCleanUp", 0, 1);
  }
}

TEST_F(ContextClustererHistoryServiceObserverTest, DeleteAllHistory) {
  base::HistogramTester histogram_tester;

  VisitURL(GURL("https://example.com"), 1, Now());
  VisitURL(GURL("https://example.com/2"), 2, Now());

  // Simulate deleting all history.
  DeleteHistory(/*urls=*/{});

  // Force a cleanup pass.
  MoveClockForwardBy(GetConfig().context_clustering_clean_up_duration);

  // There should be nothing to clean up so it shouldn't even initiate a clean
  // up pass.
  histogram_tester.ExpectTotalCount(
      "History.Clusters.ContextClusterer.NumClusters.AtCleanUp", 0);
  histogram_tester.ExpectTotalCount(
      "History.Clusters.ContextClusterer.NumClusters.CleanedUp", 0);
  histogram_tester.ExpectTotalCount(
      "History.Clusters.ContextClusterer.NumClusters.PostCleanUp", 0);

  histogram_tester.ExpectTotalCount(
      "History.Clusters.ContextClusterer.VisitProcessingLatency.UrlsDeleted",
      1);
}

TEST_F(ContextClustererHistoryServiceObserverTest, DeleteSelectURLs) {
  base::HistogramTester histogram_tester;

  VisitURL(GURL("https://example.com"), 1, Now());
  VisitURL(GURL("https://example.com/2"), 2, Now());
  VisitURL(GURL("https://default-engine.com/search?q=foo"), 3, Now());
  VisitURL(GURL("https://default-engine.com/search?q=foo#whatever"), 4, Now());

  // Simulate deleting one URL and one search URL.
  DeleteHistory({GURL("https://example.com"),
                 GURL("https://default-engine.com/search?q=foo#whateverelse")});

  // Force a cleanup pass.
  MoveClockForwardBy(GetConfig().context_clustering_clean_up_duration);

  // There should be 1 cluster untouched by the deletion.
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.ContextClusterer.NumClusters.AtCleanUp", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.ContextClusterer.NumClusters.CleanedUp", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.ContextClusterer.NumClusters.PostCleanUp", 1, 1);

  histogram_tester.ExpectTotalCount(
      "History.Clusters.ContextClusterer.VisitProcessingLatency.UrlsDeleted",
      1);
}

}  // namespace history_clusters