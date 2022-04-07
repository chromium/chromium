// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/content_annotations_cluster_processor.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/history_clusters/core/clustering_test_utils.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/on_device_clustering_features.h"
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
    config_.content_cluster_on_intersection_similarity = false;
    config_.content_clustering_similarity_threshold = 0.5;
    SetConfigForTesting(config_);
  }

  void SetUp() override {
    cluster_processor_ = std::make_unique<ContentAnnotationsClusterProcessor>();
  }

  void TearDown() override { cluster_processor_.reset(); }

  std::vector<history::Cluster> ProcessClusters(
      const std::vector<history::Cluster>& clusters) {
    return cluster_processor_->ProcessClusters(clusters);
  }

 private:
  Config config_;
  std::unique_ptr<ContentAnnotationsClusterProcessor> cluster_processor_;
};

TEST_F(ContentAnnotationsClusterProcessorTest, AboveThreshold) {
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

  std::vector<history::Cluster> result_clusters = ProcessClusters(clusters);
  EXPECT_THAT(
      testing::ToVisitResults(result_clusters),
      ElementsAre(ElementsAre(
          testing::VisitResult(1, 1.0), testing::VisitResult(2, 1.0),
          testing::VisitResult(4, 1.0), testing::VisitResult(10, 1.0))));
  ASSERT_EQ(result_clusters.size(), 1u);
}

TEST_F(ContentAnnotationsClusterProcessorTest, BelowThreshold) {
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
  visit4.content_annotations.model_annotations.categories = {{"category", 1}};
  visit4.content_annotations.model_annotations.entities = {{"github", 1}};
  history::Cluster cluster2;
  cluster2.visits = {testing::CreateClusterVisit(visit4)};
  clusters.push_back(cluster2);

  // This visit has the same entities but no categories and shouldn't be
  // grouped with the others.
  history::AnnotatedVisit visit5 = testing::CreateDefaultAnnotatedVisit(
      10, GURL("https://nonexistentreferrer.com/"));
  visit5.content_annotations.model_annotations.entities = {{"github", 1}};
  history::Cluster cluster3;
  cluster3.visits = {testing::CreateClusterVisit(visit5)};
  clusters.push_back(cluster3);

  // This visit has the same categories but no entities and shouldn't be
  // grouped with the others.
  history::AnnotatedVisit visit6 =
      testing::CreateDefaultAnnotatedVisit(11, GURL("https://othervisit.com/"));
  visit6.content_annotations.model_annotations.categories = {{"category", 1}};
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

  std::vector<history::Cluster> result_clusters = ProcessClusters(clusters);
  EXPECT_THAT(testing::ToVisitResults(result_clusters),
              ElementsAre(ElementsAre(testing::VisitResult(1, 1.0),
                                      testing::VisitResult(2, 1.0),
                                      testing::VisitResult(4, 1.0)),
                          ElementsAre(testing::VisitResult(10, 1.0)),
                          ElementsAre(testing::VisitResult(11, 1.0)),
                          ElementsAre(testing::VisitResult(12, 1.0))));
  EXPECT_THAT(result_clusters.size(), 4u);
}

class ContentAnnotationsIntersectionMetricTest
    : public ContentAnnotationsClusterProcessorTest {
 public:
  ContentAnnotationsIntersectionMetricTest() {
    config_.content_clustering_enabled = true;
    config_.content_cluster_on_intersection_similarity = true;
    SetConfigForTesting(config_);
  }

 private:
  Config config_;
};

TEST_F(ContentAnnotationsIntersectionMetricTest, AboveThreshold) {
  std::vector<history::Cluster> clusters;

  history::AnnotatedVisit visit =
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://github.com/"));
  visit.content_annotations.model_annotations.entities = {{"github", 1}};
  visit.content_annotations.model_annotations.categories = {{"category", 1}};
  history::AnnotatedVisit visit2 =
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://google.com/"));
  visit2.content_annotations.model_annotations.entities = {{"github", 1}};
  visit2.content_annotations.model_annotations.categories = {{"category2", 2}};
  history::AnnotatedVisit visit4 =
      testing::CreateDefaultAnnotatedVisit(4, GURL("https://github.com/"));
  visit4.content_annotations.model_annotations.entities = {{"github", 1}};
  history::Cluster cluster1;
  cluster1.visits = {testing::CreateClusterVisit(visit),
                     testing::CreateClusterVisit(visit2),
                     testing::CreateClusterVisit(visit4)};
  clusters.push_back(cluster1);

  // After the context clustering, visit5 will not be in the same cluster as
  // visit, visit2, and visit4 but has the same two categories, which is
  // above the default intersection threshold.
  history::AnnotatedVisit visit5 = testing::CreateDefaultAnnotatedVisit(
      10, GURL("https://nonexistentreferrer.com/"));
  visit5.content_annotations.model_annotations.entities = {{"github", 1}};
  visit5.content_annotations.model_annotations.categories = {{"category", 1}};
  history::AnnotatedVisit visit6 = testing::CreateDefaultAnnotatedVisit(
      11, GURL("https://nonexistentreferrer.com/"));
  visit6.content_annotations.model_annotations.entities = {{"github", 1}};
  visit6.content_annotations.model_annotations.categories = {{"category2", 2}};
  history::Cluster cluster2;
  cluster2.visits = {testing::CreateClusterVisit(visit5),
                     testing::CreateClusterVisit(visit6)};
  clusters.push_back(cluster2);

  std::vector<history::Cluster> result_clusters = ProcessClusters(clusters);
  EXPECT_THAT(testing::ToVisitResults(result_clusters),
              ElementsAre(ElementsAre(
                  testing::VisitResult(1, 1.0), testing::VisitResult(2, 1.0),
                  testing::VisitResult(4, 1.0), testing::VisitResult(10, 1.0),
                  testing::VisitResult(11, 1.0))));
  ASSERT_EQ(result_clusters.size(), 1u);
}

TEST_F(ContentAnnotationsIntersectionMetricTest, BelowThreshold) {
  std::vector<history::Cluster> clusters;

  history::AnnotatedVisit visit =
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://github.com/"));
  visit.content_annotations.model_annotations.entities = {{"github", 1}};
  visit.content_annotations.model_annotations.categories = {{"category", 1}};
  history::AnnotatedVisit visit2 =
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://google.com/"));
  visit2.content_annotations.model_annotations.entities = {{"github", 1}};
  visit2.content_annotations.model_annotations.categories = {{"category2", 2}};
  history::AnnotatedVisit visit4 =
      testing::CreateDefaultAnnotatedVisit(4, GURL("https://github.com/"));
  visit4.content_annotations.model_annotations.entities = {{"github", 1}};
  history::Cluster cluster1;
  cluster1.visits = {testing::CreateClusterVisit(visit),
                     testing::CreateClusterVisit(visit2),
                     testing::CreateClusterVisit(visit4)};
  clusters.push_back(cluster1);

  // After the context clustering, visit5 will not be in the same cluster as
  // visit, visit2, and visit4 and it only intersects on one categories, which
  // is below the default intersection threshold.
  history::AnnotatedVisit visit5 = testing::CreateDefaultAnnotatedVisit(
      10, GURL("https://nonexistentreferrer.com/"));
  visit5.content_annotations.model_annotations.entities = {{"github", 1}};
  visit5.content_annotations.model_annotations.categories = {{"category", 1}};
  history::Cluster cluster2;
  cluster2.visits = {testing::CreateClusterVisit(visit5)};
  clusters.push_back(cluster2);

  std::vector<history::Cluster> result_clusters = ProcessClusters(clusters);
  ASSERT_EQ(result_clusters.size(), 2u);
}

}  // namespace
}  // namespace history_clusters
