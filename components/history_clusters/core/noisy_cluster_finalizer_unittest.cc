// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/noisy_cluster_finalizer.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/history_clusters/core/clustering_test_utils.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/on_device_clustering_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_clusters {
namespace {

using ::testing::ElementsAre;

class NoisyClusterFinalizerTest : public ::testing::Test {
 public:
  void SetUp() override {
    cluster_finalizer_ = std::make_unique<NoisyClusterFinalizer>();

    config_.number_interesting_visits_filter_threshold = 2;
    SetConfigForTesting(config_);
  }

  void TearDown() override { cluster_finalizer_.reset(); }

  void FinalizeCluster(history::Cluster& cluster) {
    cluster_finalizer_->FinalizeCluster(cluster);
  }

 private:
  Config config_;
  std::unique_ptr<NoisyClusterFinalizer> cluster_finalizer_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(NoisyClusterFinalizerTest, FilterHighEngagementClusters) {
  history::ClusterVisit visit = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://bar.com/")));
  visit.engagement_score = 25.0;

  history::ClusterVisit visit2 = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://bar.com/")));
  visit2.duplicate_visits.push_back(
      testing::ClusterVisitToDuplicateClusterVisit(visit));
  visit2.engagement_score = 25.0;

  history::Cluster cluster;
  cluster.visits = {visit2};
  FinalizeCluster(cluster);
  EXPECT_FALSE(cluster.should_show_on_prominent_ui_surfaces);
}

TEST_F(NoisyClusterFinalizerTest, HideClusterWithOnlyOneInterestingVisit) {
  base::HistogramTester histogram_tester;
  history::ClusterVisit visit = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://bar.com/")));
  visit.engagement_score = 5.0;

  history::ClusterVisit visit2 = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://bar.com/")));
  visit2.duplicate_visits.push_back(
      testing::ClusterVisitToDuplicateClusterVisit(visit));
  visit2.engagement_score = 25.0;

  history::Cluster cluster;
  cluster.visits = {visit2};
  FinalizeCluster(cluster);
  EXPECT_FALSE(cluster.should_show_on_prominent_ui_surfaces);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.WasClusterFiltered.NoisyCluster", true, 1);
}

TEST_F(NoisyClusterFinalizerTest, KeepClusterWithAtLeastTwoInterestingVisits) {
  base::HistogramTester histogram_tester;
  history::ClusterVisit visit = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://bar.com/")));
  visit.engagement_score = 5.0;

  history::ClusterVisit visit2 = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://bar.com/")));
  visit2.engagement_score = 25.0;

  history::ClusterVisit visit3 = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(3, GURL("https://foo.com/")));
  visit3.duplicate_visits.push_back(
      testing::ClusterVisitToDuplicateClusterVisit(visit));
  visit3.engagement_score = 5.0;

  history::Cluster cluster;
  cluster.visits = {visit2, visit3};
  FinalizeCluster(cluster);
  EXPECT_TRUE(cluster.should_show_on_prominent_ui_surfaces);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.WasClusterFiltered.NoisyCluster", false, 1);
}

}  // namespace
}  // namespace history_clusters
