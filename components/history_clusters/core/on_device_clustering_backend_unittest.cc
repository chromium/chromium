// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/on_device_clustering_backend.h"

#include "base/containers/flat_set.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/history_clusters/core/clustering_test_utils.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/on_device_clustering_features.h"
#include "components/optimization_guide/core/entity_metadata_provider.h"
#include "components/optimization_guide/core/new_optimization_guide_decider.h"
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
  ~TestSiteEngagementScoreProvider() = default;

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

class TestEntityMetadataProvider
    : public optimization_guide::EntityMetadataProvider {
 public:
  explicit TestEntityMetadataProvider(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner)
      : main_thread_task_runner_(main_thread_task_runner) {}
  ~TestEntityMetadataProvider() override = default;

  // EntityMetadataProvider:
  void GetMetadataForEntityId(
      const std::string& entity_id,
      optimization_guide::EntityMetadataRetrievedCallback callback) override {
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](const std::string& entity_id,
               optimization_guide::EntityMetadataRetrievedCallback callback) {
              optimization_guide::EntityMetadata metadata;
              metadata.human_readable_name = "rewritten-" + entity_id;
              // Add it in twice to verify that a category only gets added once
              // and it takes the max.
              metadata.human_readable_categories.insert(
                  {"category-" + entity_id, 0.6});
              metadata.human_readable_categories.insert(
                  {"category-" + entity_id, 0.5});
              metadata.human_readable_categories.insert(
                  {"toolow-" + entity_id, 0.01});
              metadata.human_readable_aliases.push_back("alias-" + entity_id);
              std::move(callback).Run(entity_id == "nometadata"
                                          ? absl::nullopt
                                          : absl::make_optional(metadata));
            },
            entity_id, std::move(callback)));
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
};

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
};

class OnDeviceClusteringWithoutContentBackendTest : public ::testing::Test {
 public:
  OnDeviceClusteringWithoutContentBackendTest() {
    config_.content_clustering_enabled = false;
    config_.keyword_filter_on_categories = true;
    config_.keyword_filter_on_noisy_visits = true;
    config_.keyword_filter_on_entity_aliases = true;
    config_.max_entity_aliases_in_keywords = 100;
    config_.split_clusters_at_search_visits = false;
    config_.should_label_clusters = false;
    config_.entity_relevance_threshold = 60;
    config_.should_check_hosts_to_skip_clustering_for = true;
    SetConfigForTesting(config_);
  }

  void SetUp() override {
    clustering_backend_ = std::make_unique<OnDeviceClusteringBackend>(
        /*entity_metadata_provider=*/nullptr, &test_site_engagement_provider_,
        /*optimization_guide_decider_=*/nullptr,
        /*mid_blocklist_=*/base::flat_set<std::string>({"blockedentity"}));
  }

  void TearDown() override { clustering_backend_.reset(); }

  std::vector<history::Cluster> ClusterVisits(
      ClusteringRequestSource clustering_request_source,
      const std::vector<history::AnnotatedVisit>& visits) {
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
        visits);
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
}

TEST_F(OnDeviceClusteringWithoutContentBackendTest,
       ClusterTwoVisitsTiedByReferringVisit) {
  base::HistogramTester histogram_tester;
  std::vector<history::AnnotatedVisit> visits;

  // Visit2's referrer is visit 1 and are close together.
  history::AnnotatedVisit visit =
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://google.com/"));
  visit.content_annotations.model_annotations.categories = {
      {"google-category", 100}, {"com", 100}};
  visits.push_back(visit);

  history::AnnotatedVisit visit2 =
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://google.com/next"));
  visit2.content_annotations.model_annotations.entities = {
      {"google-entity", 100}, {"com", 100}};
  visit2.referring_visit_of_redirect_chain_start = 1;
  visits.push_back(visit2);

  std::vector<history::Cluster> result_clusters =
      ClusterVisits(ClusteringRequestSource::kJourneysPage, visits);
  EXPECT_THAT(testing::ToVisitResults(result_clusters),
              ElementsAre(ElementsAre(testing::VisitResult(1, 1.0),
                                      testing::VisitResult(2, 1.0))));
  ASSERT_EQ(result_clusters.size(), 1u);
  EXPECT_THAT(result_clusters.at(0).GetKeywords(),
              UnorderedElementsAre(std::u16string(u"google-category"),
                                   std::u16string(u"com"),
                                   std::u16string(u"google-entity")));
  EXPECT_FALSE(result_clusters[0].label.has_value());
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.ClusterSize.Min", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.ClusterSize.Max", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.NumKeywordsPerCluster.Min", 3, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.NumKeywordsPerCluster.Max", 3, 1);
}

TEST_F(OnDeviceClusteringWithoutContentBackendTest,
       ClusterTwoVisitsTiedByOpenerVisit) {
  base::HistogramTester histogram_tester;
  std::vector<history::AnnotatedVisit> visits;

  // Visit2's referrer is visit 1 and are close together.
  history::AnnotatedVisit visit =
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://google.com/"));
  visits.push_back(visit);

  history::AnnotatedVisit visit2 =
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://google.com/next"));
  visit2.opener_visit_of_redirect_chain_start = 1;
  visits.push_back(visit2);

  std::vector<history::Cluster> result_clusters =
      ClusterVisits(ClusteringRequestSource::kJourneysPage, visits);
  EXPECT_THAT(testing::ToVisitResults(result_clusters),
              ElementsAre(ElementsAre(testing::VisitResult(1, 1.0),
                                      testing::VisitResult(2, 1.0))));
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.ClusterSize.Min", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.ClusterSize.Max", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.NumKeywordsPerCluster.Min", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.NumKeywordsPerCluster.Max", 0, 1);
}

TEST_F(OnDeviceClusteringWithoutContentBackendTest, ClusterTwoVisitsTiedByURL) {
  base::HistogramTester histogram_tester;
  std::vector<history::AnnotatedVisit> visits;

  // Visit2 has the same URL as Visit1.
  history::AnnotatedVisit visit =
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://google.com/"));
  visits.push_back(visit);

  history::AnnotatedVisit visit2 =
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://google.com/"));
  visits.push_back(visit2);

  std::vector<history::Cluster> result_clusters =
      ClusterVisits(ClusteringRequestSource::kJourneysPage, visits);
  EXPECT_THAT(testing::ToVisitResults(result_clusters),
              ElementsAre(ElementsAre(testing::VisitResult(
                  2, 1.0, {testing::VisitResult(1, 0.0)}))));
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.ClusterSize.Min", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.ClusterSize.Max", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.NumKeywordsPerCluster.Min", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.NumKeywordsPerCluster.Max", 0, 1);
}

TEST_F(OnDeviceClusteringWithoutContentBackendTest, DedupeClusters) {
  std::vector<history::AnnotatedVisit> visits;

  // Visit2 has the same URL as Visit1.
  history::AnnotatedVisit visit =
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://google.com/"));
  visits.push_back(visit);

  history::AnnotatedVisit visit2 =
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://google.com/"));
  visits.push_back(visit2);

  std::vector<history::Cluster> result_clusters =
      ClusterVisits(ClusteringRequestSource::kJourneysPage, visits);
  EXPECT_THAT(testing::ToVisitResults(result_clusters),
              ElementsAre(ElementsAre(testing::VisitResult(
                  2, 1.0, {testing::VisitResult(1, 0.0)}))));
}

TEST_F(OnDeviceClusteringWithoutContentBackendTest,
       DedupeRespectsDifferentURLs) {
  std::vector<history::AnnotatedVisit> visits;

  // Visit2 has a different URL but is linked by referring id.
  history::AnnotatedVisit visit =
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://google.com/"));
  visits.push_back(visit);

  history::AnnotatedVisit visit2 =
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://foo.com/"));
  visit2.referring_visit_of_redirect_chain_start = 1;
  visits.push_back(visit2);

  std::vector<history::Cluster> result_clusters =
      ClusterVisits(ClusteringRequestSource::kJourneysPage, visits);
  EXPECT_THAT(testing::ToVisitResults(result_clusters),
              ElementsAre(ElementsAre(testing::VisitResult(1, 1.0),
                                      testing::VisitResult(2, 1.0))));
}

TEST_F(OnDeviceClusteringWithoutContentBackendTest, MultipleClusters) {
  base::HistogramTester histogram_tester;
  std::vector<history::AnnotatedVisit> visits;

  // Visit2's referrer is visit 1 and visit 4 is a back navigation from visit 2.
  // Visit 3 is a different journey altogether. Visit 10 is referring to a
  // missing visit and should be considered as in its own cluster.
  // Also, make sure these aren't sorted so we test that we are sorting the
  // visits by visit ID.
  history::AnnotatedVisit visit =
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://github.com/"));
  visits.push_back(visit);

  history::AnnotatedVisit visit2 =
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://google.com/"));
  visit2.referring_visit_of_redirect_chain_start = 1;
  // Set the visit duration to be 2x the default so it has the same duration
  // after |visit| and |visit4| are deduped.
  visit2.visit_row.visit_duration = base::Seconds(20);
  visits.push_back(visit2);

  history::AnnotatedVisit visit4 =
      testing::CreateDefaultAnnotatedVisit(4, GURL("https://github.com/"));
  visits.push_back(visit4);

  history::AnnotatedVisit visit5 = testing::CreateDefaultAnnotatedVisit(
      10, GURL("https://nonexistentreferrer.com/"));
  visit5.referring_visit_of_redirect_chain_start = 6;
  visits.push_back(visit5);

  // Although it says shouldskip, it should not be skipped since there is no
  // optimization guide decider.
  history::AnnotatedVisit visit3 = testing::CreateDefaultAnnotatedVisit(
      3, GURL("https://shouldskip.com/butnotsincenodecider"));
  visits.push_back(visit3);

  std::vector<history::Cluster> result_clusters =
      ClusterVisits(ClusteringRequestSource::kJourneysPage, visits);
  EXPECT_THAT(
      testing::ToVisitResults(result_clusters),
      ElementsAre(ElementsAre(testing::VisitResult(2, 1.0),
                              testing::VisitResult(
                                  4, 1.0, {testing::VisitResult(1, 0.0)})),
                  ElementsAre(testing::VisitResult(3, 1.0)),
                  ElementsAre(testing::VisitResult(10, 1.0))));
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.ClusterSize.Min", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.ClusterSize.Max", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.NumKeywordsPerCluster.Min", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.NumKeywordsPerCluster.Max", 0, 1);

  // This is coming from the Journeys page so expect that the per-cluster
  // metrics are not collected.
  histogram_tester.ExpectTotalCount(
      "History.Clusters.Backend.ClusterContainsSearch", 0);
  histogram_tester.ExpectTotalCount(
      "History.Clusters.Backend.NumKeywordsPerCluster", 0);
  histogram_tester.ExpectTotalCount(
      "History.Clusters.Backend.NumVisitsPerCluster", 0);
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
      ClusterVisits(ClusteringRequestSource::kKeywordCacheGeneration, visits);
  EXPECT_THAT(testing::ToVisitResults(result_clusters),
              ElementsAre(ElementsAre(testing::VisitResult(3, 1.0)),
                          ElementsAre(testing::VisitResult(2, 1.0),
                                      testing::VisitResult(1, 1.0))));
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.ClusterSize.Min", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.ClusterSize.Max", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.NumKeywordsPerCluster.Min", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.NumKeywordsPerCluster.Max", 0, 1);

  // This is coming from the keyword cache generation so expect that the
  // per-cluster metrics are collected.
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.ClusterContainsSearch", false, 2);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.NumKeywordsPerCluster", 0, 2);
  histogram_tester.ExpectTotalCount(
      "History.Clusters.Backend.NumVisitsPerCluster", 2);
  histogram_tester.ExpectBucketCount(
      "History.Clusters.Backend.NumVisitsPerCluster", 1, 1);
  histogram_tester.ExpectBucketCount(
      "History.Clusters.Backend.NumVisitsPerCluster", 2, 1);
}

class OnDeviceClusteringWithContentBackendTest
    : public OnDeviceClusteringWithoutContentBackendTest {
 public:
  OnDeviceClusteringWithContentBackendTest() {
    config_.content_clustering_enabled = true;
    config_.keyword_filter_on_categories = true;
    config_.keyword_filter_on_noisy_visits = true;
    config_.keyword_filter_on_entity_aliases = true;
    config_.should_check_hosts_to_skip_clustering_for = false;
    SetConfigForTesting(config_);
  }

 private:
  Config config_;
};

TEST_F(OnDeviceClusteringWithContentBackendTest, ClusterOnContent) {
  std::vector<history::AnnotatedVisit> visits;

  // Visit2's referrer is visit 1 and visit 4 is a back navigation from visit 2.
  // Visit 3 is a different journey altogether. Visit 10 is referring to a
  // missing visit and should be considered as in its own cluster.
  // Also, make sure these aren't sorted so we test that we are sorting the
  // visits by visit ID.
  history::AnnotatedVisit visit =
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://github.com/"));
  visit.content_annotations.model_annotations.entities = {{"github", 100}};
  visit.content_annotations.model_annotations.categories = {{"category", 100}};
  visits.push_back(visit);

  history::AnnotatedVisit visit2 =
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://google.com/"));
  visit2.content_annotations.model_annotations.entities = {{"github", 100}};
  visit2.content_annotations.model_annotations.categories = {{"category", 100}};
  visit2.referring_visit_of_redirect_chain_start = 1;
  // Set the visit duration to be 2x the default so it has the same duration
  // after |visit| and |visit4| are deduped.
  visit2.visit_row.visit_duration = base::Seconds(20);
  visits.push_back(visit2);

  history::AnnotatedVisit visit4 =
      testing::CreateDefaultAnnotatedVisit(4, GURL("https://github.com/"));
  visit4.content_annotations.model_annotations.entities = {{"github", 100}};
  visit4.content_annotations.model_annotations.categories = {
      {"category", 100}, {"category2", 100}};
  visits.push_back(visit4);

  // After the context clustering, visit5 will not be in the same cluster as
  // visit, visit2, and visit4 but all of the visits have the same entities
  // and categories so they will be clustered in the content pass.
  history::AnnotatedVisit visit5 = testing::CreateDefaultAnnotatedVisit(
      10, GURL("https://shouldskip.com/butnotsincehostcheckingisfalse/"
               "andhasnonexistentreferrer"));
  visit5.content_annotations.model_annotations.entities = {{"github", 100}};
  visit5.content_annotations.model_annotations.categories = {
      {"category", 100}, {"category2", 100}};
  visit5.referring_visit_of_redirect_chain_start = 6;
  visits.push_back(visit5);

  std::vector<history::Cluster> result_clusters =
      ClusterVisits(ClusteringRequestSource::kJourneysPage, visits);
  EXPECT_THAT(testing::ToVisitResults(result_clusters),
              ElementsAre(ElementsAre(
                  testing::VisitResult(2, 1.0),
                  testing::VisitResult(4, 1.0, {testing::VisitResult(1, 0.0)}),
                  testing::VisitResult(10, 0.5))));
}

TEST_F(OnDeviceClusteringWithContentBackendTest,
       ClusterOnContentBelowThreshold) {
  base::HistogramTester histogram_tester;
  std::vector<history::AnnotatedVisit> visits;

  // Visit2's referrer is visit 1 and visit 4 is a back navigation from visit 2.
  // Visit 3 is a different journey altogether. Visit 10 is referring to a
  // missing visit and should be considered as in its own cluster.
  // Also, make sure these aren't sorted so we test that we are sorting the
  // visits by visit ID.
  history::AnnotatedVisit visit =
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://github.com/"));
  visit.content_annotations.model_annotations.entities = {{"github", 100}};
  visit.content_annotations.model_annotations.categories = {{"category", 100}};
  visits.push_back(visit);

  history::AnnotatedVisit visit2 =
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://google.com/"));
  visit2.referring_visit_of_redirect_chain_start = 1;
  // Set the visit duration to be 2x the default so it has the same duration
  // after |visit| and |visit4| are deduped.
  visit2.visit_row.visit_duration = base::Seconds(20);
  visits.push_back(visit2);

  // After the context clustering, visit4 will not be in the same cluster as
  // visit and visit2 but should be clustered together since they have the same
  // title.
  history::AnnotatedVisit visit4 =
      testing::CreateDefaultAnnotatedVisit(4, GURL("https://github.com/"));
  visit4.content_annotations.model_annotations.entities = {{"github", 100}};
  visit4.content_annotations.model_annotations.categories = {{"category", 100}};
  visits.push_back(visit4);

  // This visit has a different title and shouldn't be grouped with the others.
  history::AnnotatedVisit visit5 = testing::CreateDefaultAnnotatedVisit(
      10, GURL("https://nonexistentreferrer.com/"));
  visit5.referring_visit_of_redirect_chain_start = 6;
  visit5.content_annotations.model_annotations.entities = {{"irrelevant", 100}};
  visits.push_back(visit5);

  std::vector<history::Cluster> result_clusters =
      ClusterVisits(ClusteringRequestSource::kJourneysPage, visits);
  EXPECT_THAT(
      testing::ToVisitResults(result_clusters),
      ElementsAre(ElementsAre(testing::VisitResult(2, 1.0),
                              testing::VisitResult(
                                  4, 1.0, {testing::VisitResult(1, 0.0)})),
                  ElementsAre(testing::VisitResult(10, 1.0))));
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.ClusterSize.Min", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.ClusterSize.Max", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.NumKeywordsPerCluster.Min", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.NumKeywordsPerCluster.Max", 2, 1);
}

class OnDeviceClusteringWithAllTheBackendsTest
    : public OnDeviceClusteringWithoutContentBackendTest {
 public:
  void SetUp() override {
    entity_metadata_provider_ = std::make_unique<TestEntityMetadataProvider>(
        task_environment_.GetMainThreadTaskRunner());

    optimization_guide_decider_ =
        std::make_unique<TestOptimizationGuideDecider>();

    clustering_backend_ = std::make_unique<OnDeviceClusteringBackend>(
        entity_metadata_provider_.get(),
        /*engagement_score_provider=*/nullptr,
        optimization_guide_decider_.get(),
        /*mid_blocklist_=*/base::flat_set<std::string>({"blockedentity"}));
  }

 private:
  std::unique_ptr<TestEntityMetadataProvider> entity_metadata_provider_;
  std::unique_ptr<TestOptimizationGuideDecider> optimization_guide_decider_;
};

TEST_F(OnDeviceClusteringWithAllTheBackendsTest, EntityOnMidBlocklist) {
  base::HistogramTester histogram_tester;
  std::vector<history::AnnotatedVisit> visits;

  // Visit 2 refers to visit 1 and will be clustered. Visit 3 refers to a
  // missing visit and should be considered as in its own cluster.
  // Goal is to test the mid blocklist
  history::AnnotatedVisit visit =
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://github.com/"));
  visit.content_annotations.model_annotations.entities = {
      {"blockedentity", 100}};
  visits.push_back(visit);

  history::AnnotatedVisit visit2 =
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://google.com/"));
  visit2.referring_visit_of_redirect_chain_start = 1;
  visit2.content_annotations.model_annotations.entities = {{"unblocked", 100}};
  // Set the visit duration to be 2x the default so it has the same duration
  // after |visit| and |visit4| are deduped.
  visit2.visit_row.visit_duration = base::Seconds(20);
  visits.push_back(visit2);

  // This visit has a different title and shouldn't be grouped with the others.
  history::AnnotatedVisit visit3 = testing::CreateDefaultAnnotatedVisit(
      10, GURL("https://nonexistentreferrer.com/"));
  visit3.referring_visit_of_redirect_chain_start = 6;
  visit3.content_annotations.model_annotations.entities = {{"irrelevant", 100}};
  visits.push_back(visit3);

  std::vector<history::Cluster> result_clusters =
      ClusterVisits(ClusteringRequestSource::kJourneysPage, visits);

  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.ClusterSize.Min", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.ClusterSize.Max", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.NumKeywordsPerCluster.Min", 3, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.NumKeywordsPerCluster.Max", 3, 1);
}

TEST_F(OnDeviceClusteringWithAllTheBackendsTest,
       DedupeSimilarUrlSameSearchQuery) {
  base::HistogramTester histogram_tester;
  std::vector<history::AnnotatedVisit> visits;

  // Visit2 has the same search URL as Visit1.
  history::AnnotatedVisit visit = testing::CreateDefaultAnnotatedVisit(
      1, GURL("http://default-engine.com/?q=foo&otherstuff"));
  visit.content_annotations.model_annotations.visibility_score = 0.5;
  visit.content_annotations.search_terms = u"foo";
  visit.content_annotations.search_normalized_url =
      GURL("http://default-engine.com/?q=foo");
  visits.push_back(visit);

  history::AnnotatedVisit visit2 = testing::CreateDefaultAnnotatedVisit(
      2, GURL("http://default-engine.com/?q=foo"));
  visit2.content_annotations.model_annotations.entities = {
      history::VisitContentModelAnnotations::Category("foo", 70),
      history::VisitContentModelAnnotations::Category("nometadata", 100),
      history::VisitContentModelAnnotations::Category("toolow", 1),
  };
  visit2.content_annotations.model_annotations.visibility_score = 0.5;
  visit2.content_annotations.search_terms = u"foo";
  visit2.content_annotations.search_normalized_url =
      GURL("http://default-engine.com/?q=foo");
  visits.push_back(visit2);

  history::AnnotatedVisit visit3 = testing::CreateDefaultAnnotatedVisit(
      3, GURL("http://non-default-engine.com/?q=nometadata#whatever"));
  visit3.content_annotations.model_annotations.entities = {
      history::VisitContentModelAnnotations::Category("nometadata", 100),
      // This is too low and should not be added as a keyword despite it
      // being a valid entity for a different visit.
      history::VisitContentModelAnnotations::Category("foo", 10),
  };
  visit3.content_annotations.search_terms = u"nometadata";
  visit3.content_annotations.search_normalized_url =
      GURL("http://non-default-engine.com/?q=nometadata");
  visit3.content_annotations.model_annotations.visibility_score = 0.5;
  visits.push_back(visit3);

  history::AnnotatedVisit should_skip = testing::CreateDefaultAnnotatedVisit(
      11, GURL("https://shouldskip.com/whatever"));
  visits.push_back(should_skip);

  std::vector<history::Cluster> result_clusters =
      ClusterVisits(ClusteringRequestSource::kJourneysPage, visits);
  EXPECT_THAT(
      testing::ToVisitResults(result_clusters),
      ElementsAre(
          ElementsAre(testing::VisitResult(
              2, 1.0, {testing::VisitResult(1, 0.0, {}, u"foo")}, u"foo")),
          ElementsAre(testing::VisitResult(3, 1.0, {}, u"nometadata"))));
  // Make sure visits are normalized.
  history::Cluster cluster = result_clusters.at(0);
  ASSERT_EQ(cluster.visits.size(), 1u);
  // The first visit should have its original URL as the normalized URL and
  // also have its entities rewritten.
  history::ClusterVisit better_visit = cluster.visits.at(0);
  EXPECT_EQ(better_visit.normalized_url,
            GURL("http://default-engine.com/?q=foo"));
  std::vector<history::VisitContentModelAnnotations::Category> entities =
      better_visit.annotated_visit.content_annotations.model_annotations
          .entities;
  ASSERT_EQ(entities.size(), 1u);
  EXPECT_EQ(entities.at(0).id, "rewritten-foo");
  std::vector<history::VisitContentModelAnnotations::Category> categories =
      better_visit.annotated_visit.content_annotations.model_annotations
          .categories;
  ASSERT_EQ(categories.size(), 1u);
  EXPECT_EQ(categories.at(0).id, "category-foo");
  EXPECT_EQ(categories.at(0).weight, /*70*0.6=*/42);
  EXPECT_THAT(better_visit.annotated_visit.content_annotations.model_annotations
                  .visibility_score,
              FloatEq(0.5));
  // The second visit should have a normalized URL, but be the worse duplicate.
  EXPECT_EQ(cluster.visits.at(0).duplicate_visits.at(0).normalized_url,
            GURL("http://default-engine.com/?q=foo"));
  EXPECT_THAT(cluster.visits.at(0)
                  .duplicate_visits.at(0)
                  .annotated_visit.content_annotations.model_annotations
                  .visibility_score,
              FloatEq(0.5));
  // Cluster should have 3 keywords.
  EXPECT_THAT(
      cluster.GetKeywords(),
      UnorderedElementsAre(u"rewritten-foo", u"category-foo", u"alias-foo"));

  history::Cluster cluster2 = result_clusters.at(1);
  ASSERT_EQ(cluster2.visits.size(), 1u);
  // The third visit should have its original URL as the normalized URL and
  // also have its entities rewritten.
  history::ClusterVisit third_result_visit = cluster2.visits.at(0);
  EXPECT_EQ(third_result_visit.normalized_url,
            GURL("http://non-default-engine.com/?q=nometadata"));
  EXPECT_TRUE(third_result_visit.annotated_visit.content_annotations
                  .model_annotations.entities.empty());
  EXPECT_TRUE(third_result_visit.annotated_visit.content_annotations
                  .model_annotations.categories.empty());
  EXPECT_TRUE(cluster2.keyword_to_data_map.empty());

  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.ClusterSize.Min", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.ClusterSize.Max", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.NumKeywordsPerCluster.Min", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.NumKeywordsPerCluster.Max", 3, 1);
  histogram_tester.ExpectTotalCount(
      "History.Clusters.Backend.BatchEntityLookupLatency2", 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.BatchEntityLookupSize", 2, 1);
}

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
