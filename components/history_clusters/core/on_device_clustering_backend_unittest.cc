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
#include "components/history_clusters/core/clustering_test_utils.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/history_clusters_util.h"
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
  void GetMetadataForEntityIds(
      const base::flat_set<std::string>& entity_ids,
      optimization_guide::BatchEntityMetadataRetrievedCallback callback)
      override {
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](const base::flat_set<std::string>& entity_ids,
               optimization_guide::BatchEntityMetadataRetrievedCallback
                   callback) {
              base::flat_map<std::string, optimization_guide::EntityMetadata>
                  entity_metadata_map;
              for (const auto& entity_id : entity_ids) {
                if (entity_id == "nometadata") {
                  continue;
                }
                optimization_guide::EntityMetadata metadata;
                metadata.human_readable_name = "rewritten-" + entity_id;
                // Add it in twice to verify that a category only gets added
                // once and it takes the max.
                metadata.human_readable_categories.insert(
                    {"category-" + entity_id, 0.6});
                metadata.human_readable_categories.insert(
                    {"category-" + entity_id, 0.5});
                metadata.human_readable_categories.insert(
                    {"toolow-" + entity_id, 0.01});
                metadata.human_readable_aliases.push_back("alias-" + entity_id);
                entity_metadata_map[entity_id] = metadata;
              }
              std::move(callback).Run(entity_metadata_map);
            },
            entity_ids, std::move(callback)));
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

  void CanApplyOptimizationOnDemand(
      const std::vector<GURL>& urls,
      const base::flat_set<optimization_guide::proto::OptimizationType>&
          optimization_types,
      optimization_guide::proto::RequestContext request_context,
      optimization_guide::OnDemandOptimizationGuideDecisionRepeatingCallback
          callback) override {}
};

class OnDeviceClusteringWithoutContentBackendTest : public ::testing::Test {
 public:
  OnDeviceClusteringWithoutContentBackendTest() {
    config_.content_clustering_enabled = false;
    config_.keyword_filter_on_noisy_visits = true;
    config_.keyword_filter_on_entity_aliases = true;
    config_.max_entity_aliases_in_keywords = 100;
    config_.entity_relevance_threshold = 60;
    config_.should_check_hosts_to_skip_clustering_for = true;
    config_.use_host_for_visit_deduping = false;
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

    // Sort clusters here for easier verification.
    SortClusters(&clusters);
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
  // then deduped.

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

  std::vector<history::Cluster> result_clusters =
      GetClustersForUI(ClusteringRequestSource::kJourneysPage,
                       QueryClustersFilterParams(), clusters);
  EXPECT_THAT(testing::ToVisitResults(result_clusters),
              ElementsAre(ElementsAre(testing::VisitResult(
                  2, 1.0, {history::DuplicateClusterVisit{1}}))));
  EXPECT_FALSE(result_clusters[0].label->empty());
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

class OnDeviceClusteringWithContentBackendTest
    : public OnDeviceClusteringWithoutContentBackendTest {
 public:
  OnDeviceClusteringWithContentBackendTest() {
    config_.content_clustering_enabled = true;
    config_.exclude_entities_that_have_no_collections_from_content_clustering =
        false;
    config_.collections_to_block_from_content_clustering = {};
    config_.keyword_filter_on_noisy_visits = true;
    config_.keyword_filter_on_entity_aliases = true;
    config_.should_check_hosts_to_skip_clustering_for = false;
    SetConfigForTesting(config_);
  }

  void SetUp() override {
    entity_metadata_provider_ = std::make_unique<TestEntityMetadataProvider>(
        task_environment_.GetMainThreadTaskRunner());

    clustering_backend_ = std::make_unique<OnDeviceClusteringBackend>(
        entity_metadata_provider_.get(),
        /*engagement_score_provider=*/nullptr,
        /*optimization_guide_decider=*/nullptr,
        /*mid_blocklist_=*/base::flat_set<std::string>({"blockedentity"}));
  }

 private:
  std::unique_ptr<TestEntityMetadataProvider> entity_metadata_provider_;
  Config config_;
};

TEST_F(OnDeviceClusteringWithContentBackendTest,
       ClusterNoRequiresUIAndTriggerability) {
  std::vector<history::AnnotatedVisit> visits;

  // Visit2's referrer is visit 1 and visit 4 is a back navigation from visit 2.
  // Visit 3 is a different journey altogether. Visit 10 is referring to a
  // missing visit and should be considered as in its own cluster.
  // Also, make sure these aren't sorted so we test that we are sorting the
  // visits by visit ID.
  history::AnnotatedVisit visit = testing::CreateDefaultAnnotatedVisit(
      1, GURL("https://github.com/"), base::Time::FromTimeT(1));
  visit.content_annotations.model_annotations.entities = {{"github", 100}};
  visits.push_back(visit);

  history::AnnotatedVisit visit2 = testing::CreateDefaultAnnotatedVisit(
      2, GURL("https://google.com/"), base::Time::FromTimeT(2));
  visit2.content_annotations.model_annotations.entities = {{"github", 100}};
  visit2.referring_visit_of_redirect_chain_start = 1;
  // Set the visit duration to be 2x the default so it has the same duration
  // after |visit| and |visit4| are deduped.
  visit2.visit_row.visit_duration = base::Seconds(20);
  visits.push_back(visit2);

  history::AnnotatedVisit visit4 = testing::CreateDefaultAnnotatedVisit(
      4, GURL("https://github.com/"), base::Time::FromTimeT(4));
  visit4.content_annotations.model_annotations.entities = {{"github", 100}};
  visits.push_back(visit4);

  // After the context clustering, visit5 will not be in the same cluster as
  // visit, visit2, and visit4 but all of the visits have the same entities
  // so they will be clustered in the content pass.
  history::AnnotatedVisit visit5 = testing::CreateDefaultAnnotatedVisit(
      10,
      GURL("https://shouldskip.com/butnotsincehostcheckingisfalse/"
           "andhasnonexistentreferrer"),
      base::Time::FromTimeT(10));
  visit5.content_annotations.model_annotations.entities = {{"github", 100}};
  visit5.referring_visit_of_redirect_chain_start = 6;
  visits.push_back(visit5);

  std::vector<history::Cluster> result_clusters =
      ClusterVisits(ClusteringRequestSource::kJourneysPage, visits,
                    /*requires_ui_and_triggerability=*/false);

  // The clusters should not be grouped by content and visits are not deduped or
  // scored if `requires_ui_and_triggerability` is false.
  EXPECT_THAT(testing::ToVisitResults(result_clusters),
              ElementsAre(ElementsAre(testing::VisitResult(10, 1.0)),
                          ElementsAre(testing::VisitResult(4, 1.0),
                                      testing::VisitResult(2, 1.0),
                                      testing::VisitResult(1, 1.0))));
  // The clusters should not have keywords or triggerability calculated.
  EXPECT_EQ(result_clusters.size(), 2u);
  EXPECT_TRUE(result_clusters[0].GetKeywords().empty());
  EXPECT_FALSE(result_clusters[0].triggerability_calculated);
  EXPECT_TRUE(result_clusters[1].GetKeywords().empty());
  EXPECT_FALSE(result_clusters[1].triggerability_calculated);
}

TEST_F(OnDeviceClusteringWithContentBackendTest, ClusterOnContent) {
  std::vector<history::AnnotatedVisit> visits;

  // Visit2's referrer is visit 1 and visit 4 is a back navigation from visit 2.
  // Visit 3 is a different journey altogether. Visit 10 is referring to a
  // missing visit and should be considered as in its own cluster.
  // Also, make sure these aren't sorted so we test that we are sorting the
  // visits by visit ID.
  history::AnnotatedVisit visit = testing::CreateDefaultAnnotatedVisit(
      1, GURL("https://github.com/"), base::Time::FromTimeT(1));
  visit.content_annotations.model_annotations.entities = {{"github", 100}};
  visits.push_back(visit);

  history::AnnotatedVisit visit2 = testing::CreateDefaultAnnotatedVisit(
      2, GURL("https://google.com/"), base::Time::FromTimeT(2));
  visit2.content_annotations.model_annotations.entities = {{"github", 100}};
  visit2.referring_visit_of_redirect_chain_start = 1;
  // Set the visit duration to be 2x the default so it has the same duration
  // after |visit| and |visit4| are deduped.
  visit2.visit_row.visit_duration = base::Seconds(20);
  visits.push_back(visit2);

  history::AnnotatedVisit visit4 = testing::CreateDefaultAnnotatedVisit(
      4, GURL("https://github.com/"), base::Time::FromTimeT(4));
  visit4.content_annotations.model_annotations.entities = {{"github", 100}};
  visits.push_back(visit4);

  // After the context clustering, visit5 will not be in the same cluster as
  // visit, visit2, and visit4 but all of the visits have the same entities
  // so they will be clustered in the content pass.
  history::AnnotatedVisit visit5 = testing::CreateDefaultAnnotatedVisit(
      10,
      GURL("https://shouldskip.com/butnotsincehostcheckingisfalse/"
           "andhasnonexistentreferrer"),
      base::Time::FromTimeT(10));
  visit5.content_annotations.model_annotations.entities = {{"github", 100}};
  visit5.referring_visit_of_redirect_chain_start = 6;
  visits.push_back(visit5);

  std::vector<history::Cluster> result_clusters =
      ClusterVisits(ClusteringRequestSource::kJourneysPage, visits);
  EXPECT_THAT(
      testing::ToVisitResults(result_clusters),
      ElementsAre(ElementsAre(
          testing::VisitResult(4, 1.0, {history::DuplicateClusterVisit{1}}),
          testing::VisitResult(2, 1.0), testing::VisitResult(10, 0.5))));
}

TEST_F(OnDeviceClusteringWithContentBackendTest, GetClustersForUIWithContent) {
  std::vector<history::Cluster> clusters;

  history::Cluster cluster1;
  history::AnnotatedVisit visit = testing::CreateDefaultAnnotatedVisit(
      1, GURL("https://github.com/"), base::Time::FromTimeT(1));
  visit.content_annotations.model_annotations.entities = {{"github", 100}};
  cluster1.visits.push_back(testing::CreateClusterVisit(visit));

  history::AnnotatedVisit visit2 = testing::CreateDefaultAnnotatedVisit(
      2, GURL("https://google.com/"), base::Time::FromTimeT(2));
  visit2.content_annotations.model_annotations.entities = {{"github", 100}};
  visit2.referring_visit_of_redirect_chain_start = 1;
  // Set the visit duration to be 2x the default so it has the same duration
  // after |visit| and |visit4| are deduped.
  visit2.visit_row.visit_duration = base::Seconds(20);
  cluster1.visits.push_back(testing::CreateClusterVisit(visit2));

  history::AnnotatedVisit visit4 = testing::CreateDefaultAnnotatedVisit(
      4, GURL("https://github.com/"), base::Time::FromTimeT(4));
  visit4.content_annotations.model_annotations.entities = {{"github", 100}};
  cluster1.visits.push_back(testing::CreateClusterVisit(visit4));
  clusters.push_back(cluster1);

  // After the context clustering, visit5 will not be in the same cluster as
  // visit, visit2, and visit4 but all of the visits have the same entities
  // so they will be clustered in the content pass.
  history::Cluster cluster2;
  history::AnnotatedVisit visit5 = testing::CreateDefaultAnnotatedVisit(
      10,
      GURL("https://shouldskip.com/butnotsincehostcheckingisfalse/"
           "andhasnonexistentreferrer"),
      base::Time::FromTimeT(10));
  visit5.content_annotations.model_annotations.entities = {{"github", 100}};
  visit5.referring_visit_of_redirect_chain_start = 6;
  cluster2.visits.push_back(testing::CreateClusterVisit(visit5));
  clusters.push_back(cluster2);

  std::vector<history::Cluster> result_clusters =
      GetClustersForUI(ClusteringRequestSource::kJourneysPage,
                       QueryClustersFilterParams(), clusters);
  EXPECT_THAT(
      testing::ToVisitResults(result_clusters),
      ElementsAre(ElementsAre(
          testing::VisitResult(4, 1.0, {history::DuplicateClusterVisit{1}}),
          testing::VisitResult(2, 1.0), testing::VisitResult(10, 0.5))));
  EXPECT_THAT(result_clusters.size(), 1u);
  EXPECT_THAT(result_clusters[0].GetKeywords(),
              UnorderedElementsAre(u"alias-github", u"rewritten-github"));
}

TEST_F(OnDeviceClusteringWithContentBackendTest,
       GetClusterTriggerabilityWithContent) {
  std::vector<history::Cluster> clusters;

  history::Cluster cluster1;
  history::AnnotatedVisit visit = testing::CreateDefaultAnnotatedVisit(
      1, GURL("https://github.com/"), base::Time::FromTimeT(1));
  visit.content_annotations.model_annotations.entities = {{"github", 100},
                                                          {"scoretoolow", 10}};
  cluster1.visits.push_back(testing::CreateClusterVisit(visit));

  history::AnnotatedVisit visit2 = testing::CreateDefaultAnnotatedVisit(
      2, GURL("https://google.com/"), base::Time::FromTimeT(2));
  visit2.content_annotations.model_annotations.entities = {{"github", 100}};
  visit2.referring_visit_of_redirect_chain_start = 1;
  // Set the visit duration to be 2x the default so it has the same duration
  // after |visit| and |visit4| are deduped.
  visit2.visit_row.visit_duration = base::Seconds(20);
  cluster1.visits.push_back(testing::CreateClusterVisit(visit2));

  history::AnnotatedVisit visit4 = testing::CreateDefaultAnnotatedVisit(
      4, GURL("https://github.com/"), base::Time::FromTimeT(4));
  visit4.content_annotations.model_annotations.entities = {{"github", 100},
                                                           {"nometadata", 100}};
  cluster1.visits.push_back(testing::CreateClusterVisit(visit4));
  clusters.push_back(cluster1);

  std::vector<history::Cluster> result_clusters =
      GetClusterTriggerability(clusters);
  EXPECT_THAT(result_clusters.size(), 1u);
  EXPECT_THAT(result_clusters[0].GetKeywords(),
              UnorderedElementsAre(u"alias-github", u"rewritten-github"));
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
  history::AnnotatedVisit visit = testing::CreateDefaultAnnotatedVisit(
      1, GURL("https://github.com/"), base::Time::FromTimeT(1));
  visit.content_annotations.model_annotations.entities = {{"github", 100}};
  visit.content_annotations.model_annotations.categories = {{"category", 100}};
  visits.push_back(visit);

  history::AnnotatedVisit visit2 = testing::CreateDefaultAnnotatedVisit(
      2, GURL("https://google.com/"), base::Time::FromTimeT(2));
  visit2.referring_visit_of_redirect_chain_start = 1;
  // Set the visit duration to be 2x the default so it has the same duration
  // after |visit| and |visit4| are deduped.
  visit2.visit_row.visit_duration = base::Seconds(20);
  visits.push_back(visit2);

  // After the context clustering, visit4 will not be in the same cluster as
  // visit and visit2 but should be clustered together since they have the same
  // title.
  history::AnnotatedVisit visit4 = testing::CreateDefaultAnnotatedVisit(
      4, GURL("https://github.com/"), base::Time::FromTimeT(4));
  visit4.content_annotations.model_annotations.entities = {{"github", 100}};
  visit4.content_annotations.model_annotations.categories = {{"category", 100}};
  visits.push_back(visit4);

  // This visit has a different title and shouldn't be grouped with the others.
  history::AnnotatedVisit visit5 = testing::CreateDefaultAnnotatedVisit(
      10, GURL("https://nonexistentreferrer.com/"), base::Time::FromTimeT(10));
  visit5.referring_visit_of_redirect_chain_start = 6;
  visit5.content_annotations.model_annotations.entities = {{"irrelevant", 100}};
  visits.push_back(visit5);

  std::vector<history::Cluster> result_clusters =
      ClusterVisits(ClusteringRequestSource::kJourneysPage, visits);
  EXPECT_THAT(
      testing::ToVisitResults(result_clusters),
      ElementsAre(ElementsAre(testing::VisitResult(10, 1.0)),
                  ElementsAre(testing::VisitResult(
                                  4, 1.0, {history::DuplicateClusterVisit{1}}),
                              testing::VisitResult(2, 1.0))));
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
  visit2.visit_row.visit_duration = base::Seconds(20);
  visits.push_back(visit2);

  std::vector<history::Cluster> result_clusters =
      ClusterVisits(ClusteringRequestSource::kJourneysPage, visits);
  EXPECT_EQ(result_clusters.size(), 1u);

  // Cluster 1 should have 1 keyword with the "blockedentity" being blocked.
  EXPECT_THAT(result_clusters[0].GetKeywords(),
              UnorderedElementsAre(u"alias-unblocked", u"rewritten-unblocked"));
}

TEST_F(OnDeviceClusteringWithAllTheBackendsTest,
       DedupeSimilarUrlSameSearchQuery) {
  base::HistogramTester histogram_tester;
  std::vector<history::AnnotatedVisit> visits;

  // Visit2 has the same search URL as Visit1.
  history::AnnotatedVisit visit = testing::CreateDefaultAnnotatedVisit(
      1, GURL("http://default-engine.com/?q=foo&otherstuff"),
      base::Time::FromTimeT(1));
  visit.content_annotations.model_annotations.visibility_score = 0.5;
  visit.content_annotations.search_terms = u"foo";
  visit.content_annotations.search_normalized_url =
      GURL("http://default-engine.com/?q=foo");
  visits.push_back(visit);

  history::AnnotatedVisit visit2 = testing::CreateDefaultAnnotatedVisit(
      2, GURL("http://default-engine.com/?q=foo"), base::Time::FromTimeT(2));
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
      3, GURL("http://non-default-engine.com/?q=nometadata#whatever"),
      base::Time::FromTimeT(3));
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
      11, GURL("https://shouldskip.com/whatever"), base::Time::FromTimeT(11));
  visits.push_back(should_skip);

  std::vector<history::Cluster> result_clusters =
      ClusterVisits(ClusteringRequestSource::kJourneysPage, visits);
  EXPECT_THAT(
      testing::ToVisitResults(result_clusters),
      ElementsAre(ElementsAre(testing::VisitResult(3, 1.0, {}, u"nometadata")),
                  ElementsAre(testing::VisitResult(
                      2, 1.0, {history::DuplicateClusterVisit{1}}, u"foo"))));
  // Make sure visits are normalized.
  history::Cluster cluster = result_clusters.at(0);
  ASSERT_EQ(cluster.visits.size(), 1u);
  // The third visit should have its original URL as the normalized URL and
  // also have its entities rewritten.
  history::ClusterVisit third_result_visit = cluster.visits.at(0);
  EXPECT_EQ(third_result_visit.normalized_url,
            GURL("http://non-default-engine.com/?q=nometadata"));
  EXPECT_TRUE(third_result_visit.annotated_visit.content_annotations
                  .model_annotations.entities.empty());
  EXPECT_TRUE(third_result_visit.annotated_visit.content_annotations
                  .model_annotations.categories.empty());
  // Search query terms are keywords.
  EXPECT_THAT(cluster.GetKeywords(), UnorderedElementsAre(u"nometadata"));

  history::Cluster cluster2 = result_clusters.at(1);
  ASSERT_EQ(cluster2.visits.size(), 1u);
  // The first visit should have its original URL as the normalized URL.
  history::ClusterVisit better_visit = cluster2.visits.at(0);
  EXPECT_EQ(better_visit.normalized_url,
            GURL("http://default-engine.com/?q=foo"));
  std::vector<history::VisitContentModelAnnotations::Category> entities =
      better_visit.annotated_visit.content_annotations.model_annotations
          .entities;
  ASSERT_EQ(entities.size(), 1u);
  EXPECT_EQ(entities.at(0).id, "foo");
  std::vector<history::VisitContentModelAnnotations::Category> categories =
      better_visit.annotated_visit.content_annotations.model_annotations
          .categories;
  EXPECT_TRUE(categories.empty());
  EXPECT_THAT(better_visit.annotated_visit.content_annotations.model_annotations
                  .visibility_score,
              FloatEq(0.5));
  // The second visit should have a URL.
  EXPECT_EQ(cluster2.visits.at(0).duplicate_visits.at(0).url,
            GURL("http://default-engine.com/?q=foo&otherstuff"));
  // Cluster should have 3 keywords with the search term "foo" included.
  EXPECT_THAT(cluster2.GetKeywords(),
              UnorderedElementsAre(u"rewritten-foo", u"alias-foo", u"foo"));

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
