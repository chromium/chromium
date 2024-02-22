// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/on_device_clustering_backend.h"

#include "base/containers/flat_set.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/clustering_test_utils.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/history_clusters_util.h"
#include "components/history_clusters/core/on_device_clustering_features.h"
#include "components/optimization_guide/core/test_optimization_guide_decider.h"
#include "components/site_engagement/core/site_engagement_score_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_clusters {
namespace {

using ::testing::ElementsAre;
using ::testing::FloatEq;
using ::testing::UnorderedElementsAre;

class TestSiteEngagementScoreProvider
    : public site_engagement::SiteEngagementScoreProvider {
 public:
  TestSiteEngagementScoreProvider() = default;
  ~TestSiteEngagementScoreProvider() override = default;

  double GetScore(const GURL& url) const override {
    ++count_get_score_invocations_;
    return 0;
  }

  double GetTotalEngagementPoints() const override { return 1; }

  size_t count_get_score_invocations() const {
    return count_get_score_invocations_;
  }

 private:
  mutable size_t count_get_score_invocations_ = 0;
};

class TestOptimizationGuideDecider
    : public optimization_guide::TestOptimizationGuideDecider {
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
};

class OnDeviceClusteringWithoutContentBackendTest : public ::testing::Test {
 public:
  OnDeviceClusteringWithoutContentBackendTest() {
    config_.keyword_filter_on_noisy_visits = true;
    config_.should_check_hosts_to_skip_clustering_for = true;
    config_.use_host_for_visit_deduping = false;
    SetConfigForTesting(config_);
  }

  void SetUp() override {
    clustering_backend_ = std::make_unique<OnDeviceClusteringBackend>(
        &test_site_engagement_provider_,
        /*optimization_guide_decider_=*/nullptr);
  }

  void TearDown() override { clustering_backend_.reset(); }

  std::vector<history::Cluster> ClusterVisits(
      ClusteringRequestSource clustering_request_source,
      const std::vector<history::AnnotatedVisit>& visits,
      bool requires_ui_and_triggerability = true) {
    std::vector<history::Cluster> clusters;

    base::RunLoop run_loop;
    clustering_backend_->GetClusters(
        clustering_request_source,
        base::BindOnce(
            [](base::RunLoop* run_loop,
               std::vector<history::Cluster>* out_clusters,
               std::vector<history::Cluster> clusters) {
              *out_clusters = std::move(clusters);
              run_loop->Quit();
            },
            &run_loop, &clusters),
        visits, requires_ui_and_triggerability);
    run_loop.Run();

    // Sort clusters here for easier verification.
    SortClusters(&clusters);
    return clusters;
  }

  std::vector<history::Cluster> GetClustersForUI(
      ClusteringRequestSource clustering_request_source,
      QueryClustersFilterParams filter_params,
      const std::vector<history::Cluster>& in_clusters) {
    std::vector<history::Cluster> clusters;

    base::RunLoop run_loop;
    clustering_backend_->GetClustersForUI(
        clustering_request_source, std::move(filter_params),
        base::BindOnce(
            [](base::RunLoop* run_loop,
               std::vector<history::Cluster>* out_clusters,
               std::vector<history::Cluster> clusters) {
              *out_clusters = std::move(clusters);
              run_loop->Quit();
            },
            &run_loop, &clusters),
        in_clusters);
    run_loop.Run();

    return clusters;
  }

  std::vector<history::Cluster> GetClusterTriggerability(
      const std::vector<history::Cluster>& in_clusters) {
    std::vector<history::Cluster> clusters;

    base::RunLoop run_loop;
    clustering_backend_->GetClusterTriggerability(
        base::BindOnce(
            [](base::RunLoop* run_loop,
               std::vector<history::Cluster>* out_clusters,
               std::vector<history::Cluster> clusters) {
              *out_clusters = std::move(clusters);
              run_loop->Quit();
            },
            &run_loop, &clusters),
        in_clusters);
    run_loop.Run();

    return clusters;
  }

  size_t GetSiteEngagementGetScoreInvocationCount() const {
    return test_site_engagement_provider_.count_get_score_invocations();
  }

 protected:
  std::unique_ptr<OnDeviceClusteringBackend> clustering_backend_;
  base::test::TaskEnvironment task_environment_;

 private:
  Config config_;
  TestSiteEngagementScoreProvider test_site_engagement_provider_;
};

TEST_F(OnDeviceClusteringWithoutContentBackendTest, ClusterNoVisits) {
  EXPECT_TRUE(
      ClusterVisits(ClusteringRequestSource::kJourneysPage, {}).empty());
}

TEST_F(OnDeviceClusteringWithoutContentBackendTest,
       ClusterOneVisitNoRequiresUiAndTriggerability) {
  std::vector<history::AnnotatedVisit> visits;

  // Fill in the visits vector with 1 visit.
  history::AnnotatedVisit visit =
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://google.com/"));
  visits.push_back(visit);

  std::vector<history::Cluster> result_clusters =
      ClusterVisits(ClusteringRequestSource::kJourneysPage, visits,
                    /*requires_ui_and_triggerability=*/false);
  EXPECT_THAT(testing::ToVisitResults(result_clusters),
              ElementsAre(ElementsAre(testing::VisitResult(1, 1.0))));
  // Make sure triggerability was not calculated.
  EXPECT_FALSE(result_clusters[0].triggerability_calculated);
}

TEST_F(OnDeviceClusteringWithoutContentBackendTest, ClusterOneVisit) {
  std::vector<history::AnnotatedVisit> visits;

  // Fill in the visits vector with 1 visit.
  history::AnnotatedVisit visit =
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://google.com/"));
  visits.push_back(visit);

  std::vector<history::Cluster> result_clusters =
      ClusterVisits(ClusteringRequestSource::kJourneysPage, visits);
  EXPECT_THAT(testing::ToVisitResults(result_clusters),
              ElementsAre(ElementsAre(testing::VisitResult(1, 1.0))));
  // Make sure triggerability was calculated.
  EXPECT_TRUE(result_clusters[0].triggerability_calculated);
}

TEST_F(OnDeviceClusteringWithoutContentBackendTest,
       ClusterTwoVisitsTiedByReferringVisit) {
  base::HistogramTester histogram_tester;
  std::vector<history::AnnotatedVisit> visits;

  // Visit2's referrer is visit 1 and are close together.
  history::AnnotatedVisit visit = testing::CreateDefaultAnnotatedVisit(
      1, GURL("https://google.com/"), base::Time::FromTimeT(1));
  visit.content_annotations.model_annotations.categories = {
      {"google-category", 100}, {"com", 100}};
  visits.push_back(visit);

  history::AnnotatedVisit visit2 = testing::CreateDefaultAnnotatedVisit(
      2, GURL("https://google.com/next"), base::Time::FromTimeT(2));
  visit2.content_annotations.model_annotations.entities = {
      {"google-entity", 100}, {"com", 100}};
  visit2.referring_visit_of_redirect_chain_start = 1;
  visits.push_back(visit2);

  std::vector<history::Cluster> result_clusters =
      ClusterVisits(ClusteringRequestSource::kJourneysPage, visits);
  EXPECT_THAT(testing::ToVisitResults(result_clusters),
              ElementsAre(ElementsAre(testing::VisitResult(2, 1.0),
                                      testing::VisitResult(1, 1.0))));
  ASSERT_EQ(result_clusters.size(), 1u);
}

TEST_F(OnDeviceClusteringWithoutContentBackendTest,
       ClusterTwoVisitsTiedByOpenerVisit) {
  base::HistogramTester histogram_tester;
  std::vector<history::AnnotatedVisit> visits;

  // Visit2's referrer is visit 1 and are close together.
  history::AnnotatedVisit visit = testing::CreateDefaultAnnotatedVisit(
      1, GURL("https://google.com/"), base::Time::FromTimeT(1));
  visits.push_back(visit);

  history::AnnotatedVisit visit2 = testing::CreateDefaultAnnotatedVisit(
      2, GURL("https://google.com/next"), base::Time::FromTimeT(2));
  visit2.opener_visit_of_redirect_chain_start = 1;
  visits.push_back(visit2);

  std::vector<history::Cluster> result_clusters =
      ClusterVisits(ClusteringRequestSource::kJourneysPage, visits);
  EXPECT_THAT(testing::ToVisitResults(result_clusters),
              ElementsAre(ElementsAre(testing::VisitResult(2, 1.0),
                                      testing::VisitResult(1, 1.0))));
}

TEST_F(OnDeviceClusteringWithoutContentBackendTest, ClusterTwoVisitsTiedByURL) {
  base::HistogramTester histogram_tester;
  std::vector<history::AnnotatedVisit> visits;

  // Visit2 has the same URL as Visit1.
  history::AnnotatedVisit visit = testing::CreateDefaultAnnotatedVisit(
      1, GURL("https://google.com/"), base::Time::FromTimeT(1));
  visits.push_back(visit);

  history::AnnotatedVisit visit2 = testing::CreateDefaultAnnotatedVisit(
      2, GURL("https://google.com/"), base::Time::FromTimeT(2));
  visits.push_back(visit2);

  std::vector<history::Cluster> result_clusters =
      ClusterVisits(ClusteringRequestSource::kJourneysPage, visits);
  EXPECT_THAT(testing::ToVisitResults(result_clusters),
              ElementsAre(ElementsAre(testing::VisitResult(
                  2, 1.0, {history::DuplicateClusterVisit{1}}))));
}

TEST_F(OnDeviceClusteringWithoutContentBackendTest,
       GetClustersForUISimpleCase) {
  std::vector<history::Cluster> clusters;

  // Cluster processors and finalizers should be run.

  // The below clusters contain the exact same visit so should be merged and
  // then deduped. No max is applied so clusters should be returned as is.

  history::Cluster cluster1;
  cluster1.visits.emplace_back(
      testing::CreateClusterVisit(testing::CreateDefaultAnnotatedVisit(
          1, GURL("https://google.com/"), base::Time::FromTimeT(1))));
  clusters.push_back(cluster1);

  history::Cluster cluster2;
  cluster2.visits.emplace_back(
      testing::CreateClusterVisit(testing::CreateDefaultAnnotatedVisit(
          2, GURL("https://google.com/"), base::Time::FromTimeT(2))));
  clusters.push_back(cluster2);

  history::Cluster cluster3;
  cluster3.visits.emplace_back(
      testing::CreateClusterVisit(testing::CreateDefaultAnnotatedVisit(
          3, GURL("https://othercluster.com/"), base::Time::FromTimeT(4))));
  clusters.push_back(cluster3);

  std::vector<history::Cluster> result_clusters =
      GetClustersForUI(ClusteringRequestSource::kJourneysPage,
                       QueryClustersFilterParams(), clusters);
  EXPECT_THAT(testing::ToVisitResults(result_clusters),
              ElementsAre(ElementsAre(testing::VisitResult(
                              2, 1.0, {history::DuplicateClusterVisit{1}})),
                          ElementsAre(testing::VisitResult(3, 1.0))));
  EXPECT_FALSE(result_clusters[0].label->empty());
}

TEST_F(OnDeviceClusteringWithoutContentBackendTest,
       GetClustersForUIFilterApplied) {
  std::vector<history::Cluster> clusters;

  QueryClustersFilterParams params;
  params.has_related_searches = true;

  // Cluster processors and finalizers should be run.

  history::Cluster cluster1;
  cluster1.visits.emplace_back(
      testing::CreateClusterVisit(testing::CreateDefaultAnnotatedVisit(
          1, GURL("https://google.com/"), base::Time::FromTimeT(1))));
  clusters.push_back(cluster1);

  history::Cluster cluster2;
  cluster2.visits.emplace_back(
      testing::CreateClusterVisit(testing::CreateDefaultAnnotatedVisit(
          2, GURL("https://google.com/"), base::Time::FromTimeT(2))));
  clusters.push_back(cluster2);

  history::Cluster cluster3;
  cluster3.visits.emplace_back(
      testing::CreateClusterVisit(testing::CreateDefaultAnnotatedVisit(
          3, GURL("https://othercluster.com/"), base::Time::FromTimeT(4))));
  clusters.push_back(cluster3);

  std::vector<history::Cluster> result_clusters =
      GetClustersForUI(ClusteringRequestSource::kJourneysPage,
                       std::move(params), std::move(clusters));
  EXPECT_TRUE(result_clusters.empty());
}

TEST_F(OnDeviceClusteringWithoutContentBackendTest,
       GetClusterTriggerabilitySimpleCase) {
  std::vector<history::Cluster> clusters;

  // Cluster finalizers should be run.

  history::Cluster cluster1;
  cluster1.cluster_id = 1;
  cluster1.should_show_on_prominent_ui_surfaces = false;
  cluster1.visits.emplace_back(
      testing::CreateClusterVisit(testing::CreateDefaultAnnotatedVisit(
          1, GURL("https://google.com/"), base::Time::FromTimeT(1))));
  clusters.push_back(cluster1);

  history::Cluster cluster2;
  cluster2.cluster_id = 2;
  cluster2.should_show_on_prominent_ui_surfaces = false;
  cluster2.visits.emplace_back(
      testing::CreateClusterVisit(testing::CreateDefaultAnnotatedVisit(
          3, GURL("https://google.com/2"), base::Time::FromTimeT(3))));
  cluster2.visits.emplace_back(
      testing::CreateClusterVisit(testing::CreateDefaultAnnotatedVisit(
          4, GURL("https://google.com/3"), base::Time::FromTimeT(4))));
  clusters.push_back(cluster2);

  std::vector<history::Cluster> result_clusters =
      GetClusterTriggerability(clusters);
  EXPECT_EQ(result_clusters.size(), 2u);
  history::Cluster out_cluster1 = result_clusters[0];
  EXPECT_EQ(out_cluster1.cluster_id, 1);
  EXPECT_TRUE(out_cluster1.triggerability_calculated);
  // Single visit cluster.
  EXPECT_FALSE(out_cluster1.should_show_on_prominent_ui_surfaces);
  EXPECT_TRUE(out_cluster1.label.has_value());

  history::Cluster out_cluster2 = result_clusters[1];
  EXPECT_EQ(out_cluster2.cluster_id, 2);
  EXPECT_TRUE(out_cluster2.triggerability_calculated);
  EXPECT_TRUE(out_cluster2.should_show_on_prominent_ui_surfaces);
  EXPECT_TRUE(out_cluster2.label.has_value());
}

TEST_F(OnDeviceClusteringWithoutContentBackendTest, DedupeClusters) {
  std::vector<history::AnnotatedVisit> visits;

  // Visit2 has the same URL as Visit1.
  history::AnnotatedVisit visit = testing::CreateDefaultAnnotatedVisit(
      1, GURL("https://google.com/"), base::Time::FromTimeT(1));
  visits.push_back(visit);

  history::AnnotatedVisit visit2 = testing::CreateDefaultAnnotatedVisit(
      2, GURL("https://google.com/"), base::Time::FromTimeT(2));
  visits.push_back(visit2);

  std::vector<history::Cluster> result_clusters =
      ClusterVisits(ClusteringRequestSource::kJourneysPage, visits);
  EXPECT_THAT(testing::ToVisitResults(result_clusters),
              ElementsAre(ElementsAre(testing::VisitResult(
                  2, 1.0, {history::DuplicateClusterVisit{1}}))));
}

TEST_F(OnDeviceClusteringWithoutContentBackendTest,
       DedupeRespectsDifferentURLs) {
  std::vector<history::AnnotatedVisit> visits;

  // Visit2 has a different URL but is linked by referring id.
  history::AnnotatedVisit visit = testing::CreateDefaultAnnotatedVisit(
      1, GURL("https://google.com/"), base::Time::FromTimeT(1));
  visits.push_back(visit);

  history::AnnotatedVisit visit2 = testing::CreateDefaultAnnotatedVisit(
      2, GURL("https://foo.com/"), base::Time::FromTimeT(2));
  visit2.referring_visit_of_redirect_chain_start = 1;
  visits.push_back(visit2);

  std::vector<history::Cluster> result_clusters =
      ClusterVisits(ClusteringRequestSource::kJourneysPage, visits);
  EXPECT_THAT(testing::ToVisitResults(result_clusters),
              ElementsAre(ElementsAre(testing::VisitResult(2, 1.0),
                                      testing::VisitResult(1, 1.0))));
}

TEST_F(OnDeviceClusteringWithoutContentBackendTest, MultipleClusters) {
  base::HistogramTester histogram_tester;
  std::vector<history::AnnotatedVisit> visits;

  // Visit2's referrer is visit 1 and visit 4 is a back navigation from visit 2.
  // Visit 3 is a different journey altogether. Visit 10 is referring to a
  // missing visit and should be considered as in its own cluster.
  // Also, make sure these aren't sorted so we test that we are sorting the
  // visits by visit ID.
  history::AnnotatedVisit visit = testing::CreateDefaultAnnotatedVisit(
      1, GURL("https://github.com/"), base::Time::FromTimeT(1));
  visits.push_back(visit);

  history::AnnotatedVisit visit2 = testing::CreateDefaultAnnotatedVisit(
      2, GURL("https://google.com/"), base::Time::FromTimeT(2));
  visit2.referring_visit_of_redirect_chain_start = 1;
  // Set the visit duration to be 2x the default so it has the same duration
  // after |visit| and |visit4| are deduped.
  visit2.visit_row.visit_duration = base::Seconds(20);
  visits.push_back(visit2);

  history::AnnotatedVisit visit4 = testing::CreateDefaultAnnotatedVisit(
      4, GURL("https://github.com/"), base::Time::FromTimeT(4));
  visits.push_back(visit4);

  history::AnnotatedVisit visit5 = testing::CreateDefaultAnnotatedVisit(
      10, GURL("https://nonexistentreferrer.com/"), base::Time::FromTimeT(10));
  visit5.referring_visit_of_redirect_chain_start = 6;
  visits.push_back(visit5);

  // Although it says shouldskip, it should not be skipped since there is no
  // optimization guide decider.
  history::AnnotatedVisit visit3 = testing::CreateDefaultAnnotatedVisit(
      3, GURL("https://shouldskip.com/butnotsincenodecider"),
      base::Time::FromTimeT(3));
  visits.push_back(visit3);

  std::vector<history::Cluster> result_clusters =
      ClusterVisits(ClusteringRequestSource::kJourneysPage, visits);
  EXPECT_THAT(
      testing::ToVisitResults(result_clusters),
      ElementsAre(ElementsAre(testing::VisitResult(10, 1.0)),
                  ElementsAre(testing::VisitResult(
                                  4, 1.0, {history::DuplicateClusterVisit{1}}),
                              testing::VisitResult(2, 1.0)),
                  ElementsAre(testing::VisitResult(3, 1.0))));
}

TEST_F(OnDeviceClusteringWithoutContentBackendTest,
       SplitClusterOnNavigationTime) {
  base::HistogramTester histogram_tester;
  std::vector<history::AnnotatedVisit> visits;

  history::AnnotatedVisit visit =
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://google.com/"));
  visit.visit_row.visit_time = base::Time::Now();
  visits.push_back(visit);

  // Visit2 has a different URL but is linked by referring id to visit.
  history::AnnotatedVisit visit2 =
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://bar.com/"));
  visit2.referring_visit_of_redirect_chain_start = 1;
  visit2.visit_row.visit_time = base::Time::Now() + base::Minutes(5);
  visits.push_back(visit2);

  // Visit3 has a different URL but is linked by referring id to visit but the
  // cutoff has passed so it should be in a different cluster.
  history::AnnotatedVisit visit3 =
      testing::CreateDefaultAnnotatedVisit(3, GURL("https://foo.com/"));
  visit3.referring_visit_of_redirect_chain_start = 1;
  visit3.visit_row.visit_time = base::Time::Now() + base::Hours(2);
  visits.push_back(visit3);

  std::vector<history::Cluster> result_clusters =
      ClusterVisits(ClusteringRequestSource::kJourneysPage, visits);
  EXPECT_THAT(testing::ToVisitResults(result_clusters),
              ElementsAre(ElementsAre(testing::VisitResult(3, 1.0)),
                          ElementsAre(testing::VisitResult(2, 1.0),
                                      testing::VisitResult(1, 1.0))));
}

class OnDeviceClusteringWithAllTheBackendsTest
    : public OnDeviceClusteringWithoutContentBackendTest {
 public:
  void SetUp() override {
    optimization_guide_decider_ =
        std::make_unique<TestOptimizationGuideDecider>();

    clustering_backend_ = std::make_unique<OnDeviceClusteringBackend>(
        /*engagement_score_provider=*/nullptr,
        optimization_guide_decider_.get());
  }

 private:
  std::unique_ptr<TestOptimizationGuideDecider> optimization_guide_decider_;
};

TEST_F(OnDeviceClusteringWithoutContentBackendTest, EngagementScoreCache) {
  base::HistogramTester histogram_tester;
  std::vector<history::AnnotatedVisit> visits;

  // Add 2 different hosts to |visits|.
  history::AnnotatedVisit visit1 =
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://github.com/"));
  visits.push_back(visit1);

  history::AnnotatedVisit visit2 =
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://github.com/"));
  visits.push_back(visit2);

  history::AnnotatedVisit visit3 =
      testing::CreateDefaultAnnotatedVisit(4, GURL("https://github.com/"));
  visits.push_back(visit3);

  history::AnnotatedVisit visit4 =
      testing::CreateDefaultAnnotatedVisit(10, GURL("https://github.com/"));
  visits.push_back(visit4);

  history::AnnotatedVisit visit5 =
      testing::CreateDefaultAnnotatedVisit(3, GURL("https://github2.com/"));
  visits.push_back(visit5);

  std::vector<history::Cluster> result_clusters_1 =
      ClusterVisits(ClusteringRequestSource::kJourneysPage, visits);
  EXPECT_EQ(2u, GetSiteEngagementGetScoreInvocationCount());

  // No new queries should be issued when cache store is enabled.
  std::vector<history::Cluster> result_clusters_2 =
      ClusterVisits(ClusteringRequestSource::kJourneysPage, visits);
  EXPECT_EQ(2u, GetSiteEngagementGetScoreInvocationCount());
}

}  // namespace
}  // namespace history_clusters
