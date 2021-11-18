// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/ranking_cluster_finalizer.h"

#include "base/test/task_environment.h"
#include "components/history_clusters/core/clustering_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_clusters {
namespace {

using ::testing::ElementsAre;

class RankingClusterFinalizerTest : public ::testing::Test {
 public:
  void SetUp() override {
    cluster_finalizer_ = std::make_unique<RankingClusterFinalizer>();
  }

  void TearDown() override { cluster_finalizer_.reset(); }

  void FinalizeCluster(history::Cluster& cluster) {
    cluster_finalizer_->FinalizeCluster(cluster);
  }

 private:
  std::unique_ptr<RankingClusterFinalizer> cluster_finalizer_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(RankingClusterFinalizerTest, ScoreTwoVisitsSameURL) {
  // Visit2 has the same URL as Visit1.
  history::ClusterVisit visit = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://bar.com/")));

  history::ClusterVisit visit2 = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://bar.com/")));
  visit2.duplicate_visit_ids.push_back(1);

  history::Cluster cluster;
  cluster.visits = {visit, visit2};
  FinalizeCluster(cluster);
  EXPECT_THAT(testing::ToVisitResults({cluster}),
              ElementsAre(ElementsAre(testing::VisitResult(1, 0.0),
                                      testing::VisitResult(2, 1.0, {1}))));
}

TEST_F(RankingClusterFinalizerTest, ScoreTwoVisitsDifferentURLs) {
  history::ClusterVisit visit = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://bar.com/")));

  history::ClusterVisit visit2 = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://foo.com/")));

  history::Cluster cluster;
  cluster.visits = {visit, visit2};
  FinalizeCluster(cluster);
  EXPECT_THAT(testing::ToVisitResults({cluster}),
              ElementsAre(ElementsAre(testing::VisitResult(1, 1.0),
                                      testing::VisitResult(2, 1.0))));
}

TEST_F(RankingClusterFinalizerTest, ScoreTwoVisitsSimilarURL) {
  // Visit2 has the same normalized URL as Visit1.
  history::ClusterVisit visit = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(
          1, GURL("https://example.com/normalized?q=whatever")),
      GURL("https://example.com/normalized"));

  history::ClusterVisit visit2 =
      testing::CreateClusterVisit(testing::CreateDefaultAnnotatedVisit(
          2, GURL("https://example.com/normalized")));
  visit2.duplicate_visit_ids.push_back(1);

  history::Cluster cluster;
  cluster.visits = {visit, visit2};
  FinalizeCluster(cluster);
  EXPECT_THAT(testing::ToVisitResults({cluster}),
              ElementsAre(ElementsAre(testing::VisitResult(1, 0.0),
                                      testing::VisitResult(2, 1.0, {1}))));
}

TEST_F(RankingClusterFinalizerTest, ScoreMultipleVisitsDifferentDurations) {
  history::ClusterVisit visit = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://github.com/")));
  visit.annotated_visit.visit_row.visit_duration = base::Seconds(10);

  history::ClusterVisit visit2 = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://bar.com/")));
  visit2.annotated_visit.visit_row.referring_visit = 1;
  // Set the visit duration to be 2x the default so it has the same duration
  // after |visit| and |visit4| are deduped.
  visit2.annotated_visit.visit_row.visit_duration = base::Seconds(20);

  history::ClusterVisit visit4 = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(4, GURL("https://github.com/")));
  visit4.annotated_visit.visit_row.visit_duration = base::Seconds(20);
  visit4.duplicate_visit_ids.push_back(1);

  history::ClusterVisit visit5 =
      testing::CreateClusterVisit(testing::CreateDefaultAnnotatedVisit(
          10, GURL("https://nonexistentreferrer.com/")));
  visit5.annotated_visit.visit_row.referring_visit = 6;
  visit5.annotated_visit.visit_row.visit_duration = base::Seconds(10);

  history::Cluster cluster;
  cluster.visits = {visit, visit2, visit4, visit5};
  FinalizeCluster(cluster);
  EXPECT_THAT(
      testing::ToVisitResults({cluster}),
      ElementsAre(ElementsAre(
          testing::VisitResult(1, 0.0), testing::VisitResult(2, 1.0),
          testing::VisitResult(4, 1.0, {1}), testing::VisitResult(10, 0.5))));
}

TEST_F(RankingClusterFinalizerTest, ScoreTwoVisitsSameURLBookmarked) {
  // Visit2 has the same URL as Visit1.
  history::ClusterVisit visit = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://bar.com/")));
  visit.annotated_visit.context_annotations.is_existing_bookmark = true;

  history::ClusterVisit visit2 = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://bar.com/")));
  visit2.duplicate_visit_ids.push_back(1);

  history::Cluster cluster;
  cluster.visits = {visit, visit2};
  FinalizeCluster(cluster);
  EXPECT_THAT(testing::ToVisitResults({cluster}),
              ElementsAre(ElementsAre(testing::VisitResult(1, 0.0),
                                      testing::VisitResult(2, 1.0, {1}))));
}

TEST_F(RankingClusterFinalizerTest, ScoreTwoVisitsWithBookmarksAndDuration) {
  // Visit2 has the same URL as Visit1.
  history::ClusterVisit visit = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://bar.com/")));
  visit.annotated_visit.context_annotations.is_existing_bookmark = true;
  visit.annotated_visit.visit_row.visit_duration = base::Seconds(20);

  history::ClusterVisit visit2 = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://foo.com/")));
  visit2.annotated_visit.context_annotations.is_existing_bookmark = true;
  visit2.annotated_visit.visit_row.visit_duration = base::Seconds(0);

  history::Cluster cluster;
  cluster.visits = {visit, visit2};
  FinalizeCluster(cluster);
  EXPECT_THAT(testing::ToVisitResults({cluster}),
              ElementsAre(ElementsAre(testing::VisitResult(1, 1.0),
                                      testing::VisitResult(2, 0.5))));
}

TEST_F(RankingClusterFinalizerTest,
       ScoreTwoVisitsWithBookmarksAndForegroundDuration) {
  // Visit2 has the same URL as Visit1.
  history::ClusterVisit visit = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://bar.com/")));
  visit.annotated_visit.context_annotations.is_existing_bookmark = true;
  visit.annotated_visit.context_annotations.total_foreground_duration =
      base::Seconds(20);
  visit.annotated_visit.visit_row.visit_duration = base::Seconds(0);

  history::ClusterVisit visit2 = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://foo.com/")));
  visit2.annotated_visit.context_annotations.is_existing_bookmark = true;
  visit2.annotated_visit.context_annotations.total_foreground_duration =
      base::Seconds(-1);
  visit2.annotated_visit.visit_row.visit_duration = base::Seconds(0);

  history::Cluster cluster;
  cluster.visits = {visit, visit2};
  FinalizeCluster(cluster);
  EXPECT_THAT(testing::ToVisitResults({cluster}),
              ElementsAre(ElementsAre(testing::VisitResult(1, 1.0),
                                      // Foreground duration( weight=1.5) +
                                      // bookmark(weight=1) 1(1.5) + 1 = 2.5
                                      // 0(1.5) + 1 = 1
                                      // max score = 2.5
                                      // visit2 = 1 / max(score)
                                      testing::VisitResult(2, 1.0 / 2.5))));
}

TEST_F(RankingClusterFinalizerTest, ScoreTwoCanonicalSearchResultsPages) {
  // Visit2 has the same normalized URL as Visit1.
  history::ClusterVisit visit = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(
          1, GURL("https://google.com/search?q=whatever#abc")),
      GURL("https://google.com/search?q=whatever"));
  visit.is_search_visit = true;

  history::ClusterVisit visit2 = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(
          2, GURL("https://google.com/search?q=bar#abc")),
      GURL("https://google.com/search?q=bar"));
  visit2.is_search_visit = true;

  history::Cluster cluster;
  cluster.visits = {visit, visit2};
  FinalizeCluster(cluster);
  EXPECT_THAT(testing::ToVisitResults({cluster}),
              ElementsAre(ElementsAre(testing::VisitResult(1, 1.0, {}, true),
                                      testing::VisitResult(2, 1.0, {}, true))));
}

TEST_F(RankingClusterFinalizerTest, ScoreSearchResultsPagesOneDuplicate) {
  // Visit2 is marked as a duplicate of visit
  history::ClusterVisit visit = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(
          1, GURL("https://google.com/search?q=whatever#abc")),
      GURL("https://google.com/search?q=whatever"));
  visit.duplicate_visit_ids = {2};
  visit.is_search_visit = true;

  history::ClusterVisit visit2 = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(
          2, GURL("https://google.com/search?q=bar")),
      GURL("https://google.com/search?q=bar"));
  visit2.is_search_visit = true;

  history::Cluster cluster;
  cluster.visits = {visit, visit2};
  FinalizeCluster(cluster);
  EXPECT_THAT(testing::ToVisitResults({cluster}),
              ElementsAre(ElementsAre(testing::VisitResult(1, 1.0, {2}, true),
                                      testing::VisitResult(2, 0.0, {}, true))));
}

}  // namespace
}  // namespace history_clusters
