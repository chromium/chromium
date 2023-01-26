// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/content_annotations_cluster_processor.h"

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/history_clusters/core/clustering_test_utils.h"
#include "components/history_clusters/core/config.h"
#include "components/optimization_guide/core/entity_metadata.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_clusters {
namespace {

using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

class ContentAnnotationsClusterProcessorTest : public ::testing::Test {
 public:
  ContentAnnotationsClusterProcessorTest() {
    config_.content_clustering_enabled = true;
    config_.content_clustering_similarity_threshold = 0.5;
    config_.exclude_entities_that_have_no_collections_from_content_clustering =
        false;
    config_.use_pairwise_merge = true;
    config_.collections_to_block_from_content_clustering = {};
    SetConfigForTesting(config_);
  }

  void SetUp() override {
    optimization_guide::EntityMetadata github_md;
    github_md.human_readable_name = "readable-github";
    github_md.human_readable_aliases = {"git hub", "github llc"};
    github_md.collections = {"/collection/computer", "/collection/programming"};
    entity_metadata_map_["github"] = github_md;
    optimization_guide::EntityMetadata other_md;
    other_md.human_readable_name = "readable-otherentity";
    other_md.collections = {"/collections/blocked"};
    entity_metadata_map_["otherentity"] = other_md;
    optimization_guide::EntityMetadata baz_md;
    baz_md.human_readable_name = "baz";
    entity_metadata_map_["baz"] = baz_md;
    optimization_guide::EntityMetadata search_md;
    search_md.human_readable_name = "search";
    entity_metadata_map_["search"] = search_md;
    optimization_guide::EntityMetadata noisy_md;
    noisy_md.human_readable_name = "readable-onlyinnoisyvisit";
    entity_metadata_map_["onlyinnoisyvisit"] = noisy_md;
    cluster_processor_ = std::make_unique<ContentAnnotationsClusterProcessor>(
        &entity_metadata_map_);
  }

  void TearDown() override { cluster_processor_.reset(); }

  void ProcessClusters(std::vector<history::Cluster>* clusters) {
    cluster_processor_->ProcessClusters(clusters);
  }

 private:
  Config config_;
  base::flat_map<std::string, optimization_guide::EntityMetadata>
      entity_metadata_map_;
  std::unique_ptr<ContentAnnotationsClusterProcessor> cluster_processor_;
};

TEST_F(ContentAnnotationsClusterProcessorTest, AboveThreshold) {
  base::HistogramTester histogram_tester;

  std::vector<history::Cluster> clusters;

  history::AnnotatedVisit visit =
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://github.com/"));
  visit.content_annotations.model_annotations.entities = {{"github", 1}};
  visit.content_annotations.model_annotations.categories = {{"category", 1}};
  history::AnnotatedVisit visit2 =
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://google.com/"));
  visit2.content_annotations.model_annotations.entities = {{"github", 1}};
  history::AnnotatedVisit visit4 =
      testing::CreateDefaultAnnotatedVisit(4, GURL("https://github.com/"));
  visit4.content_annotations.model_annotations.entities = {{"github", 1}};
  history::Cluster cluster1;
  cluster1.visits = {testing::CreateClusterVisit(visit),
                     testing::CreateClusterVisit(visit2),
                     testing::CreateClusterVisit(visit4)};
  clusters.push_back(cluster1);

  // After the context clustering, visit5 will not be in the same cluster as
  // visit, visit2, and visit4 but all of the visits have the same entity
  // so they will be clustered in the content pass.
  history::AnnotatedVisit visit5 = testing::CreateDefaultAnnotatedVisit(
      10, GURL("https://nonexistentreferrer.com/"));
  visit5.content_annotations.model_annotations.entities = {{"github", 1}};
  visit5.content_annotations.model_annotations.categories = {{"category", 1}};
  history::Cluster cluster2;
  cluster2.visits = {testing::CreateClusterVisit(visit5)};
  clusters.push_back(cluster2);

  // After the context clustering, visit5 will not be in the same cluster as
  // visit, visit2, and visit4 but all of the visits have the same entity
  // so they will be clustered in the content pass.
  history::AnnotatedVisit visit6 = testing::CreateDefaultAnnotatedVisit(
      6, GURL("https://nonexistentreferrer.com/"));
  visit6.content_annotations.model_annotations.entities = {{"github", 1}};
  history::Cluster cluster3;
  cluster3.visits = {testing::CreateClusterVisit(visit6)};
  clusters.push_back(cluster3);

  ProcessClusters(&clusters);
  EXPECT_THAT(testing::ToVisitResults(clusters),
              ElementsAre(ElementsAre(
                  testing::VisitResult(1, 1.0), testing::VisitResult(2, 1.0),
                  testing::VisitResult(4, 1.0), testing::VisitResult(10, 1.0),
                  testing::VisitResult(6, 1.0))));
  ASSERT_EQ(clusters.size(), 1u);

  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.ContentClustering.NumClustersBeforeMerge", 3,
      1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.ContentClustering.NumClustersAfterMerge", 1, 1);
}

TEST_F(ContentAnnotationsClusterProcessorTest, BelowThreshold) {
  base::HistogramTester histogram_tester;

  std::vector<history::Cluster> clusters;

  history::AnnotatedVisit visit =
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://github.com/"));
  visit.content_annotations.model_annotations.entities = {{"github", 1}};
  visit.content_annotations.model_annotations.categories = {{"category", 1}};

  history::AnnotatedVisit visit2 =
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://google.com/"));
  visit2.visit_row.visit_duration = base::Seconds(20);
  history::Cluster cluster1;
  cluster1.visits = {testing::CreateClusterVisit(visit),
                     testing::CreateClusterVisit(visit2)};
  clusters.push_back(cluster1);

  // After the context clustering, visit4 will not be in the same cluster as
  // visit and visit2 but should be clustered together since they have the same
  // entities.
  history::AnnotatedVisit visit4 =
      testing::CreateDefaultAnnotatedVisit(4, GURL("https://github.com/"));
  visit4.content_annotations.model_annotations.entities = {{"github", 1}};
  history::Cluster cluster2;
  cluster2.visits = {testing::CreateClusterVisit(visit4)};
  clusters.push_back(cluster2);

  // This visit has no entities and shouldn't be grouped with the others.
  history::AnnotatedVisit visit6 =
      testing::CreateDefaultAnnotatedVisit(11, GURL("https://othervisit.com/"));
  history::Cluster cluster4;
  cluster4.visits = {testing::CreateClusterVisit(visit6)};
  clusters.push_back(cluster4);

  // This visit has no content annotations and shouldn't be grouped with the
  // others.
  history::AnnotatedVisit visit7 = testing::CreateDefaultAnnotatedVisit(
      12, GURL("https://nocontentannotations.com/"));
  history::Cluster cluster5;
  cluster5.visits = {testing::CreateClusterVisit(visit7)};
  clusters.push_back(cluster5);

  ProcessClusters(&clusters);
  EXPECT_THAT(testing::ToVisitResults(clusters),
              ElementsAre(ElementsAre(testing::VisitResult(1, 1.0),
                                      testing::VisitResult(2, 1.0),
                                      testing::VisitResult(4, 1.0)),
                          ElementsAre(testing::VisitResult(11, 1.0)),
                          ElementsAre(testing::VisitResult(12, 1.0))));

  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.ContentClustering.NumClustersBeforeMerge", 4,
      1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.ContentClustering.NumClustersAfterMerge", 3, 1);
}

class ContentAnnotationsSimilarityVariousCollectionsFilteringTest
    : public ContentAnnotationsClusterProcessorTest {
 public:
  ContentAnnotationsSimilarityVariousCollectionsFilteringTest() {
    config_.content_clustering_enabled = true;
    config_.exclude_entities_that_have_no_collections_from_content_clustering =
        true;
    config_.collections_to_block_from_content_clustering = {
        "/collections/blocked"};
    SetConfigForTesting(config_);
  }

 private:
  Config config_;
};

TEST_F(ContentAnnotationsSimilarityVariousCollectionsFilteringTest,
       BoWShouldBeEmptyAndNotClustered) {
  std::vector<history::Cluster> clusters;

  history::AnnotatedVisit visit =
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://github.com/"));
  visit.content_annotations.model_annotations.entities = {{"github", 1}};
  history::AnnotatedVisit visit2 =
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://google.com/"));
  visit2.content_annotations.model_annotations.entities = {{"baz", 1}};
  history::AnnotatedVisit visit4 =
      testing::CreateDefaultAnnotatedVisit(4, GURL("https://github.com/"));
  visit4.content_annotations.model_annotations.entities = {{"otherentity", 1}};
  history::Cluster cluster1;
  cluster1.visits = {testing::CreateClusterVisit(visit),
                     testing::CreateClusterVisit(visit2),
                     testing::CreateClusterVisit(visit4)};
  clusters.push_back(cluster1);

  // After the context clustering, visit5 will not be in the same cluster as
  // visit, visit2, and visit4. Ahtough 2/3 of the entities are the same as the
  // first cluster, they are not compliant with the configuration. Hence, the
  // bag for visit5 will be empty and this cluster will not be merged.
  history::AnnotatedVisit visit5 = testing::CreateDefaultAnnotatedVisit(
      10, GURL("https://nonexistentreferrer.com/"));
  visit5.content_annotations.model_annotations.entities = {{"otherentity", 1},
                                                           {"baz", 1}};
  history::Cluster cluster2;
  cluster2.visits = {testing::CreateClusterVisit(visit5)};
  clusters.push_back(cluster2);

  ProcessClusters(&clusters);
  EXPECT_THAT(testing::ToVisitResults(clusters),
              ElementsAre(ElementsAre(testing::VisitResult(1, 1.0),
                                      testing::VisitResult(2, 1.0),
                                      testing::VisitResult(4, 1.0)),
                          ElementsAre(testing::VisitResult(10, 1.0))));
}

}  // namespace
}  // namespace history_clusters
