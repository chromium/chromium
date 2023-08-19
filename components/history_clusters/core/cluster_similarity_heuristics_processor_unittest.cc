// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/cluster_similarity_heuristics_processor.h"

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/history_clusters/core/clustering_test_utils.h"
#include "components/history_clusters/core/config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_clusters {
namespace {

using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

class ClusterSimilarityHeuristicsProcessorTest : public ::testing::Test {
 public:
  void SetUp() override {
    cluster_processor_ =
        std::make_unique<ClusterSimilarityHeuristicsProcessor>();
  }

  void TearDown() override { cluster_processor_.reset(); }

  void ProcessClusters(std::vector<history::Cluster>* clusters) {
    cluster_processor_->ProcessClusters(clusters);
  }

 private:
  std::unique_ptr<ClusterSimilarityHeuristicsProcessor> cluster_processor_;
};

TEST_F(ClusterSimilarityHeuristicsProcessorTest, Merged) {
  std::vector<history::Cluster> clusters;

  history::AnnotatedVisit visit =
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://github.com/"));
  history::AnnotatedVisit visit2 =
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://google.com/"));
  history::AnnotatedVisit visit4 =
      testing::CreateDefaultAnnotatedVisit(4, GURL("https://github.com/"));
  history::Cluster cluster1;
  cluster1.visits = {testing::CreateClusterVisit(visit),
                     testing::CreateClusterVisit(visit2),
                     testing::CreateClusterVisit(visit4)};
  clusters.push_back(cluster1);

  // After the context clustering, visit5 will not be in the same cluster as
  // visit, visit2, and visit4 but all of the visits are in the first cluster.
  history::AnnotatedVisit visit5 =
      testing::CreateDefaultAnnotatedVisit(10, GURL("https://github.com/"));
  history::Cluster cluster2;
  cluster2.visits = {testing::CreateClusterVisit(visit5)};
  clusters.push_back(cluster2);

  ProcessClusters(&clusters);
  EXPECT_THAT(
      testing::ToVisitResults(clusters),
      ElementsAre(ElementsAre(
          testing::VisitResult(1, 1.0), testing::VisitResult(2, 1.0),
          testing::VisitResult(4, 1.0), testing::VisitResult(10, 1.0))));
  ASSERT_EQ(clusters.size(), 1u);
}

TEST_F(ClusterSimilarityHeuristicsProcessorTest, MergedSameSearchTerms) {
  base::HistogramTester histogram_tester;

  std::vector<history::Cluster> clusters;

  history::AnnotatedVisit visit =
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://github.com/"));
  history::AnnotatedVisit visit2 =
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://google.com/"));
  history::AnnotatedVisit visit3 =
      testing::CreateDefaultAnnotatedVisit(3, GURL("https://github.com/"));
  visit3.content_annotations.search_terms = u"search term";
  history::Cluster cluster1;
  cluster1.visits = {testing::CreateClusterVisit(visit),
                     testing::CreateClusterVisit(visit2),
                     testing::CreateClusterVisit(visit3)};
  clusters.push_back(cluster1);

  // After the context clustering, visit5 will not be in the same cluster as
  // visit, visit2, and visit4 but has the same search term as visit4.
  history::AnnotatedVisit visit5 =
      testing::CreateDefaultAnnotatedVisit(5, GURL("https://nomatch.com/"));
  visit5.content_annotations.search_terms = u"search term";
  history::Cluster cluster2;
  cluster2.visits = {testing::CreateClusterVisit(visit5)};
  clusters.push_back(cluster2);

  ProcessClusters(&clusters);
  EXPECT_THAT(testing::ToVisitResults(clusters),
              ElementsAre(ElementsAre(
                  testing::VisitResult(1, 1.0), testing::VisitResult(2, 1.0),
                  testing::VisitResult(3, 1.0, {}, u"search term"),
                  testing::VisitResult(5, 1.0, {}, u"search term"))));
  ASSERT_EQ(clusters.size(), 1u);

  // Each processed cluster only had at most one search visit.
  histogram_tester.ExpectTotalCount(
      "History.Clusters.Backend.ClusterSimilarityHeuristicsProcessor."
      "ClusterSearchTermOverriden",
      0);
}

TEST_F(ClusterSimilarityHeuristicsProcessorTest, NotMerged) {
  base::HistogramTester histogram_tester;

  std::vector<history::Cluster> clusters;

  history::AnnotatedVisit visit7 =
      testing::CreateDefaultAnnotatedVisit(12, GURL("https://github.com/"));
  history::Cluster cluster5;
  cluster5.visits = {testing::CreateClusterVisit(visit7)};
  clusters.push_back(cluster5);

  // Should be clustered with the first cluster since everything in the first
  // cluster is contained in this one.
  history::AnnotatedVisit visit =
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://github.com/"));
  history::AnnotatedVisit visit2 =
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://google.com/"));
  history::Cluster cluster1;
  cluster1.visits = {testing::CreateClusterVisit(visit),
                     testing::CreateClusterVisit(visit2)};
  clusters.push_back(cluster1);

  // Not clustered together even though there is 1 overlapping visit.
  history::AnnotatedVisit visit4 =
      testing::CreateDefaultAnnotatedVisit(4, GURL("https://github.com/"));
  history::AnnotatedVisit visit5 = testing::CreateDefaultAnnotatedVisit(
      5, GURL("https://somethingelse.com/"));
  history::Cluster cluster2;
  cluster2.visits = {testing::CreateClusterVisit(visit4),
                     testing::CreateClusterVisit(visit5)};
  clusters.push_back(cluster2);

  // Not clustered together as there is no other cluster with same search terms.
  history::AnnotatedVisit visit16 =
      testing::CreateDefaultAnnotatedVisit(16, GURL("https://github.com/"));
  visit16.content_annotations.search_terms = u"search term 1";
  history::AnnotatedVisit visit17 = testing::CreateDefaultAnnotatedVisit(
      17, GURL("https://someotherthing.com/"));
  visit17.content_annotations.search_terms = u"search term 1";
  history::Cluster cluster16;
  cluster16.visits = {testing::CreateClusterVisit(visit16),
                      testing::CreateClusterVisit(visit17)};
  clusters.push_back(cluster16);

  // Not clustered together as there is no other cluster with same search terms.
  history::AnnotatedVisit visit18 =
      testing::CreateDefaultAnnotatedVisit(18, GURL("https://different.com/"));
  visit18.content_annotations.search_terms = u"search term 2";
  history::AnnotatedVisit visit19 = testing::CreateDefaultAnnotatedVisit(
      19, GURL("https://someotherthing.com/"));
  visit19.content_annotations.search_terms = u"search term 2";
  history::Cluster cluster17;
  cluster17.visits = {testing::CreateClusterVisit(visit18),
                      testing::CreateClusterVisit(visit19)};
  clusters.push_back(cluster17);

  // This is a whole different visit.
  history::AnnotatedVisit visit6 =
      testing::CreateDefaultAnnotatedVisit(11, GURL("https://othervisit.com/"));
  history::Cluster cluster4;
  cluster4.visits = {testing::CreateClusterVisit(visit6)};
  clusters.push_back(cluster4);

  // Should be clustered with the first cluster since everything in the
  // resulting first cluster after initial merge is contained in this one.
  history::AnnotatedVisit visit15 =
      testing::CreateDefaultAnnotatedVisit(15, GURL("https://google.com/"));
  history::Cluster cluster15;
  cluster15.visits = {testing::CreateClusterVisit(visit15)};
  clusters.push_back(cluster15);

  ProcessClusters(&clusters);
  EXPECT_THAT(
      testing::ToVisitResults(clusters),
      ElementsAre(
          ElementsAre(
              testing::VisitResult(12, 1.0), testing::VisitResult(1, 1.0),
              testing::VisitResult(2, 1.0), testing::VisitResult(15, 1.0)),
          ElementsAre(testing::VisitResult(4, 1.0),
                      testing::VisitResult(5, 1.0)),
          ElementsAre(testing::VisitResult(16, 1.0, {}, u"search term 1"),
                      testing::VisitResult(17, 1.0, {}, u"search term 1")),
          ElementsAre(testing::VisitResult(18, 1.0, {}, u"search term 2"),
                      testing::VisitResult(19, 1.0, {}, u"search term 2")),
          ElementsAre(testing::VisitResult(11, 1.0))));

  // Both processed clusters had 2 search visits that matched the originally
  // determined search term.
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.ClusterSimilarityHeuristicsProcessor."
      "ClusterSearchTermOverriden",
      false, 2);
}

}  // namespace
}  // namespace history_clusters
