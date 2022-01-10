// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/on_device_clustering_backend.h"

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/history_clusters/core/clustering_test_utils.h"
#include "components/history_clusters/core/on_device_clustering_features.h"
#include "components/optimization_guide/core/entity_metadata_provider.h"
#include "components/search_engines/template_url_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_clusters {
namespace {

using ::testing::ElementsAre;
using ::testing::FloatEq;
using ::testing::UnorderedElementsAre;

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
              // Add it in twice to verify that a category only gets added once.
              metadata.human_readable_categories.insert(
                  {"category-" + entity_id, 0.5});
              metadata.human_readable_categories.insert(
                  {"category-" + entity_id, 0.5});
              std::move(callback).Run(entity_id == "nometadata"
                                          ? absl::nullopt
                                          : absl::make_optional(metadata));
            },
            entity_id, std::move(callback)));
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
};

const TemplateURLService::Initializer kTemplateURLData[] = {
    {"default-engine.com", "http://default-engine.com?q={searchTerms}",
     "Default"},
    {"non-default-engine.com", "http://non-default-engine.com?q={searchTerms}",
     "Not Default"},
};
const char16_t kDefaultTemplateURLKeyword[] = u"default-engine.com";

class OnDeviceClusteringWithoutContentBackendTest : public ::testing::Test {
 public:
  OnDeviceClusteringWithoutContentBackendTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kOnDeviceClustering,
        {{"content_clustering_enabled", "false"},
         {"dedupe_similar_visits", "false"},
         {"min_page_topics_model_version_for_visibility", "125"},
         {"include_categories_in_keywords", "true"},
         {"exclude_keywords_from_noisy_visits", "false"}});
  }

  void SetUp() override {
    clustering_backend_ = std::make_unique<OnDeviceClusteringBackend>(
        /*template_url_service=*/nullptr, /*entity_metadata_provider=*/nullptr,
        /*engagement_score_provider=*/nullptr);
  }

  void TearDown() override { clustering_backend_.reset(); }

  std::vector<history::Cluster> ClusterVisits(
      const std::vector<history::AnnotatedVisit>& visits) {
    std::vector<history::Cluster> clusters;

    base::RunLoop run_loop;
    clustering_backend_->GetClusters(
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

 protected:
  std::unique_ptr<OnDeviceClusteringBackend> clustering_backend_;
  base::test::TaskEnvironment task_environment_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(OnDeviceClusteringWithoutContentBackendTest, ClusterNoVisits) {
  EXPECT_TRUE(ClusterVisits({}).empty());
}

TEST_F(OnDeviceClusteringWithoutContentBackendTest, ClusterOneVisit) {
  std::vector<history::AnnotatedVisit> visits;

  // Fill in the visits vector with 1 visit.
  history::AnnotatedVisit visit =
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://google.com/"));
  visits.push_back(visit);

  std::vector<history::Cluster> result_clusters = ClusterVisits(visits);
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
      {"google-category", 1}, {"com", 1}};
  visits.push_back(visit);

  history::AnnotatedVisit visit2 =
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://google.com/next"));
  visit2.content_annotations.model_annotations.entities = {{"google-entity", 1},
                                                           {"com", 1}};
  visit2.referring_visit_of_redirect_chain_start = 1;
  visits.push_back(visit2);

  std::vector<history::Cluster> result_clusters = ClusterVisits(visits);
  EXPECT_THAT(testing::ToVisitResults(result_clusters),
              ElementsAre(ElementsAre(testing::VisitResult(1, 1.0),
                                      testing::VisitResult(2, 1.0))));
  ASSERT_EQ(result_clusters.size(), 1u);
  EXPECT_THAT(result_clusters.at(0).keywords,
              UnorderedElementsAre(std::u16string(u"google-category"),
                                   std::u16string(u"com"),
                                   std::u16string(u"google-entity")));
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

  std::vector<history::Cluster> result_clusters = ClusterVisits(visits);
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

  std::vector<history::Cluster> result_clusters = ClusterVisits(visits);
  EXPECT_THAT(testing::ToVisitResults(result_clusters),
              ElementsAre(ElementsAre(testing::VisitResult(1, 0.0),
                                      testing::VisitResult(2, 1.0, {1}))));
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.ClusterSize.Min", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.ClusterSize.Max", 2, 1);
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

  std::vector<history::Cluster> result_clusters = ClusterVisits(visits);
  EXPECT_THAT(testing::ToVisitResults(result_clusters),
              ElementsAre(ElementsAre(testing::VisitResult(1, 0.0),
                                      testing::VisitResult(2, 1.0, {1}))));
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

  std::vector<history::Cluster> result_clusters = ClusterVisits(visits);
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

  history::AnnotatedVisit visit3 =
      testing::CreateDefaultAnnotatedVisit(3, GURL("https://whatever.com/"));
  visits.push_back(visit3);

  std::vector<history::Cluster> result_clusters = ClusterVisits(visits);
  EXPECT_THAT(testing::ToVisitResults(result_clusters),
              ElementsAre(ElementsAre(testing::VisitResult(1, 0.0),
                                      testing::VisitResult(2, 1.0),
                                      testing::VisitResult(4, 1.0, {1})),
                          ElementsAre(testing::VisitResult(3, 1.0)),
                          ElementsAre(testing::VisitResult(10, 1.0))));
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.ClusterSize.Min", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.ClusterSize.Max", 3, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.NumKeywordsPerCluster.Min", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.NumKeywordsPerCluster.Max", 0, 1);
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

  std::vector<history::Cluster> result_clusters = ClusterVisits(visits);
  EXPECT_THAT(testing::ToVisitResults(result_clusters),
              ElementsAre(ElementsAre(testing::VisitResult(1, 1.0),
                                      testing::VisitResult(2, 1.0)),
                          ElementsAre(testing::VisitResult(3, 1.0))));
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.ClusterSize.Min", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.ClusterSize.Max", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.NumKeywordsPerCluster.Min", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.NumKeywordsPerCluster.Max", 0, 1);
}

class OnDeviceClusteringWithContentBackendTest
    : public OnDeviceClusteringWithoutContentBackendTest {
 public:
  OnDeviceClusteringWithContentBackendTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kOnDeviceClustering,
        {{"content_clustering_enabled", "true"},
         {"dedupe_similar_visits", "false"},
         {"include_categories_in_keywords", "true"},
         {"exclude_keywords_from_noisy_visits", "false"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
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
  visit.content_annotations.model_annotations.entities = {{"github", 1}};
  visit.content_annotations.model_annotations.categories = {{"category", 1}};
  visits.push_back(visit);

  history::AnnotatedVisit visit2 =
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://google.com/"));
  visit2.content_annotations.model_annotations.entities = {{"github", 1}};
  visit2.content_annotations.model_annotations.categories = {{"category", 1}};
  visit2.referring_visit_of_redirect_chain_start = 1;
  // Set the visit duration to be 2x the default so it has the same duration
  // after |visit| and |visit4| are deduped.
  visit2.visit_row.visit_duration = base::Seconds(20);
  visits.push_back(visit2);

  history::AnnotatedVisit visit4 =
      testing::CreateDefaultAnnotatedVisit(4, GURL("https://github.com/"));
  visit4.content_annotations.model_annotations.entities = {{"github", 1}};
  visit4.content_annotations.model_annotations.categories = {{"category", 1},
                                                             {"category2", 1}};
  visits.push_back(visit4);

  // After the context clustering, visit5 will not be in the same cluster as
  // visit, visit2, and visit4 but all of the visits have the same entities
  // and categories so they will be clustered in the content pass.
  history::AnnotatedVisit visit5 = testing::CreateDefaultAnnotatedVisit(
      10, GURL("https://nonexistentreferrer.com/"));
  visit5.content_annotations.model_annotations.entities = {{"github", 1}};
  visit5.content_annotations.model_annotations.categories = {{"category", 1},
                                                             {"category2", 1}};
  visit5.referring_visit_of_redirect_chain_start = 6;
  visits.push_back(visit5);

  std::vector<history::Cluster> result_clusters = ClusterVisits(visits);
  EXPECT_THAT(
      testing::ToVisitResults(result_clusters),
      ElementsAre(ElementsAre(
          testing::VisitResult(1, 0.0), testing::VisitResult(2, 1.0),
          testing::VisitResult(4, 1.0, {1}), testing::VisitResult(10, 0.5))));
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
  visit.content_annotations.model_annotations.entities = {{"github", 1}};
  visit.content_annotations.model_annotations.categories = {{"category", 1}};
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
  visit4.content_annotations.model_annotations.entities = {{"github", 1}};
  visit4.content_annotations.model_annotations.categories = {{"category", 1}};
  visits.push_back(visit4);

  // This visit has a different title and shouldn't be grouped with the others.
  history::AnnotatedVisit visit5 = testing::CreateDefaultAnnotatedVisit(
      10, GURL("https://nonexistentreferrer.com/"));
  visit5.referring_visit_of_redirect_chain_start = 6;
  visit5.content_annotations.model_annotations.entities = {{"irrelevant", 1}};
  visits.push_back(visit5);

  std::vector<history::Cluster> result_clusters = ClusterVisits(visits);
  EXPECT_THAT(testing::ToVisitResults(result_clusters),
              ElementsAre(ElementsAre(testing::VisitResult(1, 0.0),
                                      testing::VisitResult(2, 1.0),
                                      testing::VisitResult(4, 1.0, {1})),
                          ElementsAre(testing::VisitResult(10, 1.0))));
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.ClusterSize.Min", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.ClusterSize.Max", 3, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.NumKeywordsPerCluster.Min", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.NumKeywordsPerCluster.Max", 2, 1);
}

class OnDeviceClusteringWithAllTheBackendsTest
    : public OnDeviceClusteringWithoutContentBackendTest {
 public:
  void SetUp() override {
    // Set up a simple template URL service with a default search engine.
    template_url_service_ = std::make_unique<TemplateURLService>(
        kTemplateURLData, base::size(kTemplateURLData));
    TemplateURL* template_url = template_url_service_->GetTemplateURLForKeyword(
        kDefaultTemplateURLKeyword);
    template_url_service_->SetUserSelectedDefaultSearchProvider(template_url);

    entity_metadata_provider_ = std::make_unique<TestEntityMetadataProvider>(
        task_environment_.GetMainThreadTaskRunner());

    clustering_backend_ = std::make_unique<OnDeviceClusteringBackend>(
        template_url_service_.get(), entity_metadata_provider_.get(),
        /*engagement_score_provider=*/nullptr);
  }

 private:
  std::unique_ptr<TemplateURLService> template_url_service_;
  std::unique_ptr<TestEntityMetadataProvider> entity_metadata_provider_;
};

TEST_F(OnDeviceClusteringWithAllTheBackendsTest,
       DedupeSimilarUrlSameSearchQuery) {
  base::HistogramTester histogram_tester;
  std::vector<history::AnnotatedVisit> visits;

  // Visit2 has the same search URL as Visit1.
  history::AnnotatedVisit visit = testing::CreateDefaultAnnotatedVisit(
      1, GURL("http://default-engine.com/?q=foo&otherstuff"));
  visit.content_annotations.model_annotations.page_topics_model_version = 123;
  visit.content_annotations.model_annotations.visibility_score = 0.5;
  visits.push_back(visit);

  history::AnnotatedVisit visit2 = testing::CreateDefaultAnnotatedVisit(
      2, GURL("http://default-engine.com/?q=foo"));
  visit2.content_annotations.model_annotations.entities = {
      history::VisitContentModelAnnotations::Category("foo", 50),
      history::VisitContentModelAnnotations::Category("nometadata", 30),
  };
  visit2.content_annotations.model_annotations.page_topics_model_version = 127;
  visit2.content_annotations.model_annotations.visibility_score = 0.5;
  visits.push_back(visit2);

  history::AnnotatedVisit visit3 = testing::CreateDefaultAnnotatedVisit(
      3, GURL("http://non-default-engine.com/?q=nometadata#whatever"));
  visit2.content_annotations.model_annotations.entities = {
      history::VisitContentModelAnnotations::Category("nometadata", 30),
  };
  visit3.content_annotations.model_annotations.page_topics_model_version = 127;
  visit3.content_annotations.model_annotations.visibility_score = 0.5;
  visits.push_back(visit3);

  std::vector<history::Cluster> result_clusters = ClusterVisits(visits);
  ASSERT_EQ(result_clusters.size(), 2u);
  EXPECT_THAT(testing::ToVisitResults(result_clusters),
              ElementsAre(ElementsAre(testing::VisitResult(1, 0.0, {}, true),
                                      testing::VisitResult(2, 1.0, {1}, true)),
                          ElementsAre(testing::VisitResult(3, 1.0, {}, true))));
  // Make sure visits are normalized.
  history::Cluster cluster = result_clusters.at(0);
  ASSERT_EQ(cluster.visits.size(), 2u);
  // The first visit should have a normalized URL.
  EXPECT_EQ(cluster.visits.at(0).normalized_url,
            GURL("http://default-engine.com/?q=foo"));
  EXPECT_THAT(cluster.visits.at(0)
                  .annotated_visit.content_annotations.model_annotations
                  .visibility_score,
              FloatEq(-1.0));
  // The second visit should have its original URL as the normalized URL and
  // also have its entities rewritten.
  history::ClusterVisit second_result_visit = cluster.visits.at(1);
  EXPECT_EQ(second_result_visit.normalized_url,
            GURL("http://default-engine.com/?q=foo"));
  std::vector<history::VisitContentModelAnnotations::Category> entities =
      second_result_visit.annotated_visit.content_annotations.model_annotations
          .entities;
  ASSERT_EQ(entities.size(), 1u);
  EXPECT_EQ(entities.at(0).id, "rewritten-foo");
  std::vector<history::VisitContentModelAnnotations::Category> categories =
      second_result_visit.annotated_visit.content_annotations.model_annotations
          .categories;
  ASSERT_EQ(categories.size(), 1u);
  EXPECT_EQ(categories.at(0).id, "category-foo");
  EXPECT_THAT(second_result_visit.annotated_visit.content_annotations
                  .model_annotations.visibility_score,
              FloatEq(0.5));

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

  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.ClusterSize.Min", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.ClusterSize.Max", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.NumKeywordsPerCluster.Min", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.NumKeywordsPerCluster.Max", 2, 1);
  histogram_tester.ExpectTotalCount(
      "History.Clusters.Backend.BatchEntityLookupLatency", 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.BatchEntityLookupSize", 2, 1);
}

}  // namespace
}  // namespace history_clusters
