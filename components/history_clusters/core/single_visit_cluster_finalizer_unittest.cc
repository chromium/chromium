// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/single_visit_cluster_finalizer.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/history_clusters/core/clustering_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_clusters {
namespace {

class SingleVisitClusterFinalizerTest : public ::testing::Test {
 public:
  void SetUp() override {
    cluster_finalizer_ = std::make_unique<SingleVisitClusterFinalizer>();
  }

  void TearDown() override { cluster_finalizer_.reset(); }

  void FinalizeCluster(history::Cluster& cluster) {
    cluster_finalizer_->FinalizeCluster(cluster);
  }

 private:
  std::unique_ptr<SingleVisitClusterFinalizer> cluster_finalizer_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(SingleVisitClusterFinalizerTest, OneVisit) {
  history::ClusterVisit visit = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://google.com/")));

  history::Cluster cluster;
  cluster.visits = {visit};
  FinalizeCluster(cluster);
  EXPECT_FALSE(cluster.should_show_on_prominent_ui_surfaces);
}

TEST_F(SingleVisitClusterFinalizerTest,
       MultipleVisitsButExplicitlyNotShownBefore) {
  // Visit2 has the same URL as Visit1.
  history::ClusterVisit visit = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://google.com/")));
  history::ClusterVisit visit2 = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://foo.com/")));

  history::Cluster cluster;
  cluster.visits = {visit, visit2};
  cluster.should_show_on_prominent_ui_surfaces = false;
  FinalizeCluster(cluster);
  EXPECT_FALSE(cluster.should_show_on_prominent_ui_surfaces);
}

TEST_F(SingleVisitClusterFinalizerTest, MultipleVisits) {
  base::HistogramTester histogram_tester;
  history::ClusterVisit visit = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://google.com/")));
  history::ClusterVisit visit2 = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://foo.com/")));

  history::Cluster cluster;
  cluster.visits = {visit, visit2};
  FinalizeCluster(cluster);
  EXPECT_TRUE(cluster.should_show_on_prominent_ui_surfaces);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.WasClusterFiltered.SingleVisit", false, 1);
}

TEST_F(SingleVisitClusterFinalizerTest,
       MultipleVisitsButDuplicatesOfCanonical) {
  base::HistogramTester histogram_tester;
  history::ClusterVisit visit = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://google.com/")));
  history::ClusterVisit visit2 = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://foo.com/")));
  visit2.duplicate_visits = {
      testing::ClusterVisitToDuplicateClusterVisit(visit)};

  history::Cluster cluster;
  cluster.visits = {visit2};
  FinalizeCluster(cluster);
  EXPECT_FALSE(cluster.should_show_on_prominent_ui_surfaces);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.WasClusterFiltered.SingleVisit", true, 1);
}

}  // namespace
}  // namespace history_clusters
