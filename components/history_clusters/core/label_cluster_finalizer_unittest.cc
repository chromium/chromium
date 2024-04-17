// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/label_cluster_finalizer.h"

#include "base/test/task_environment.h"
#include "components/history_clusters/core/clustering_test_utils.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/on_device_clustering_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_clusters {
namespace {

using ::testing::UnorderedElementsAre;
using LabelSource = history::Cluster::LabelSource;

class LabelClusterFinalizerTest : public ::testing::Test {
 public:
  void SetUp() override {
    cluster_finalizer_ = std::make_unique<LabelClusterFinalizer>();
  }

  void TearDown() override { cluster_finalizer_.reset(); }

  void FinalizeCluster(history::Cluster& cluster) {
    cluster_finalizer_->FinalizeCluster(cluster);
  }

 private:
  std::unique_ptr<LabelClusterFinalizer> cluster_finalizer_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(LabelClusterFinalizerTest, ClusterWithNoSearchTerms) {
  history::ClusterVisit visit = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://foo.com/")));
  visit.score = 0.8;
  visit.annotated_visit.content_annotations.model_annotations.entities = {
      {"baz", 50}};

  history::ClusterVisit visit2 = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://bar.com/")));
  visit2.score = 0.25;
  visit2.annotated_visit.content_annotations.model_annotations.entities = {
      {"baz", 50}, {"highscoringentitybutlowvisitscore", 100}};

  history::ClusterVisit visit3 = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(3, GURL("https://baz.com/")));
  visit3.duplicate_visits.push_back(
      testing::ClusterVisitToDuplicateClusterVisit(visit));
  visit3.score = 0.8;
  visit3.annotated_visit.content_annotations.model_annotations.entities = {
      {"baz", 25}, {"someotherentity", 10}};

  {
    history::Cluster cluster;
    cluster.visits = {visit2, visit3};
    FinalizeCluster(cluster);
    EXPECT_EQ(cluster.raw_label, u"baz.com");
    EXPECT_EQ(cluster.label, u"baz.com and more");
    EXPECT_EQ(cluster.label_source, LabelSource::kHostname);
  }
}

TEST_F(LabelClusterFinalizerTest, TakesHighestScoringSearchTermIfAvailable) {
  // Verify that search terms take precedence.
  history::ClusterVisit visit =
      testing::CreateClusterVisit(testing::CreateDefaultAnnotatedVisit(
          2, GURL("https://nosearchtermsbuthighscorevisit.com/")));
  visit.engagement_score = 0.9;

  history::ClusterVisit visit2 =
      testing::CreateClusterVisit(testing::CreateDefaultAnnotatedVisit(
          1, GURL("https://lowerscoringsearchterm.com/")));
  visit2.score = 0.6;
  visit2.annotated_visit.content_annotations.search_terms = u"lowscore";

  history::ClusterVisit visit3 = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://baz.com/")));
  visit3.score = 0.8;
  visit3.annotated_visit.content_annotations.search_terms = u"searchtermlabel";

  history::Cluster cluster;
  cluster.visits = {visit, visit2, visit3};
  FinalizeCluster(cluster);
  EXPECT_THAT(cluster.raw_label, u"searchtermlabel");
  EXPECT_THAT(cluster.label, u"“searchtermlabel”");
  EXPECT_EQ(cluster.label_source, LabelSource::kSearch);
}

}  // namespace
}  // namespace history_clusters
