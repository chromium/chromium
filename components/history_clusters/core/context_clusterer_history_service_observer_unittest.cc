// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/context_clusterer_history_service_observer.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/history_clusters/core/config.h"
#include "components/optimization_guide/core/new_optimization_guide_decider.h"
#include "components/search_engines/template_url_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_clusters {

namespace {

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

}  // namespace

class ContextClustererHistoryServiceObserverTest : public testing::Test {
 public:
  ContextClustererHistoryServiceObserverTest() = default;
  ~ContextClustererHistoryServiceObserverTest() override = default;

  void SetUp() override {
    // Set up a simple template URL service with a default search engine.
    template_url_service_ = std::make_unique<TemplateURLService>(
        kTemplateURLData, std::size(kTemplateURLData));

    optimization_guide_decider_ =
        std::make_unique<TestOptimizationGuideDecider>();

    // Instantiate observer.
    observer_ = std::make_unique<ContextClustererHistoryServiceObserver>(
        /*history_service=*/nullptr, template_url_service_.get(),
        optimization_guide_decider_.get());
    observer_->OverrideClockForTesting(task_environment_.GetMockClock());
  }

  // Simulates a visit to URL.
  void VisitURL(const GURL& url,
                history::VisitID visit_id,
                base::Time visit_time,
                history::VisitID opener_visit = history::kInvalidVisitID,
                history::VisitID referring_visit = history::kInvalidVisitID,
                bool is_known_to_sync = false) {
    history::URLRow url_row(url);
    history::VisitRow new_visit;
    new_visit.visit_id = visit_id;
    new_visit.visit_time = visit_time;
    new_visit.opener_visit = opener_visit;
    new_visit.referring_visit = referring_visit;
    new_visit.is_known_to_sync = is_known_to_sync;
    observer_->OnURLVisited(/*history_service=*/nullptr, url_row, new_visit);
  }

  // Simulates deleting `urls` from history. If `urls` is empty, we will
  // simulate deleting all history.
  void DeleteHistory(const std::vector<GURL>& urls) {
    history::DeletionInfo deletion_info =
        urls.empty() ? history::DeletionInfo::ForAllHistory()
                     : history::DeletionInfo::ForUrls(CreateURLRows(urls),
                                                      /*favicon_urls=*/{});
    observer_->OnURLsDeleted(/*history_service=*/nullptr, deletion_info);
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

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<TemplateURLService> template_url_service_;
  std::unique_ptr<TestOptimizationGuideDecider> optimization_guide_decider_;
  std::unique_ptr<ContextClustererHistoryServiceObserver> observer_;
};

TEST_F(ContextClustererHistoryServiceObserverTest, ClusterOneVisit) {
  VisitURL(GURL("https://example.com"), 1, base::Time::FromTimeT(123));

  EXPECT_EQ(1, GetNumClustersCreated());
}

TEST_F(ContextClustererHistoryServiceObserverTest,
       ClusterTwoVisitsTiedByReferringVisit) {
  VisitURL(GURL("https://example.com"), 1, base::Time::FromTimeT(123));
  VisitURL(GURL("https://example.com/2"), 2, base::Time::FromTimeT(123),
           /*opener_visit=*/history::kInvalidVisitID, /*referring_visit=*/1);

  EXPECT_EQ(1, GetNumClustersCreated());
}

TEST_F(ContextClustererHistoryServiceObserverTest,
       ClusterTwoVisitsTiedByOpenerVisit) {
  VisitURL(GURL("https://example.com"), 1, base::Time::FromTimeT(123));
  VisitURL(GURL("https://example.com/2"), 2, base::Time::FromTimeT(123),
           /*opener_visit=*/1, /*referring_visit=*/history::kInvalidVisitID);

  EXPECT_EQ(1, GetNumClustersCreated());
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
       ClusterTwoVisitsTiedBySearchURL) {
  VisitURL(GURL("http://default-engine.com/search?q=foo"), 1,
           base::Time::FromTimeT(123));
  VisitURL(GURL("http://default-engine.com/search?q=foo#whatever"), 2,
           base::Time::FromTimeT(123),
           /*opener_visit=*/history::kInvalidVisitID,
           /*referring_visit=*/history::kInvalidVisitID);

  EXPECT_EQ(1, GetNumClustersCreated());
}

TEST_F(ContextClustererHistoryServiceObserverTest, SplitClusterOnSearchTerm) {
  VisitURL(GURL("http://default-engine.com/search?q=foo"), 1,
           base::Time::FromTimeT(123));
  VisitURL(GURL("http://default-engine.com/search?q=otherterm"), 2,
           base::Time::FromTimeT(123),
           /*opener_visit=*/1);

  EXPECT_EQ(2, GetNumClustersCreated());
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

TEST_F(ContextClustererHistoryServiceObserverTest, SkipsSyncedVisits) {
  VisitURL(GURL("https://example.com"), 1, base::Time::FromTimeT(123),
           history::kInvalidVisitID, history::kInvalidVisitID,
           /*is_known_to_sync=*/true);

  EXPECT_EQ(0, GetNumClustersCreated());
}

TEST_F(ContextClustererHistoryServiceObserverTest, SkipsBlocklistedHost) {
  VisitURL(GURL("https://shouldskip.com"), 1, base::Time::FromTimeT(123));

  EXPECT_EQ(0, GetNumClustersCreated());
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
}

}  // namespace history_clusters