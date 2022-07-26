// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/metrics_cluster_finalizer.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/history_clusters/core/clustering_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_clusters {
namespace {

using ::testing::ElementsAre;

class MetricsClusterFinalizerTest : public ::testing::Test {
 public:
  void SetUp() override {
    cluster_finalizer_ = std::make_unique<MetricsClusterFinalizer>();
  }

  void TearDown() override { cluster_finalizer_.reset(); }

  void FinalizeCluster(history::Cluster& cluster) {
    cluster_finalizer_->FinalizeCluster(cluster);
  }

 private:
  std::unique_ptr<MetricsClusterFinalizer> cluster_finalizer_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(MetricsClusterFinalizerTest, ContainsSearch) {
  base::HistogramTester histogram_tester;

  history::ClusterVisit visit = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://bar.com/")));
  visit.engagement_score = 25.0;

  history::ClusterVisit visit2 = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://bar.com/")));
  visit2.duplicate_visits.push_back(visit);
  visit2.engagement_score = 25.0;
  visit2.annotated_visit.content_annotations.search_terms = u"bar";

  history::Cluster cluster;
  cluster.visits = {visit2};
  cluster.keyword_to_data_map[u"bar"] = history::ClusterKeywordData();
  FinalizeCluster(cluster);

  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.ClusterContainsSearch", true, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.NumKeywordsPerCluster", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.NumVisitsPerCluster", 1, 1);
}

TEST_F(MetricsClusterFinalizerTest, DoesNotContainSearch) {
  base::HistogramTester histogram_tester;

  history::ClusterVisit visit = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://bar.com/")));
  visit.engagement_score = 5.0;

  history::ClusterVisit visit2 = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://bar.com/")));
  visit2.duplicate_visits.push_back(visit);
  visit2.engagement_score = 25.0;

  history::Cluster cluster;
  cluster.visits = {visit2};
  FinalizeCluster(cluster);

  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.ClusterContainsSearch", false, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.NumKeywordsPerCluster", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.NumVisitsPerCluster", 1, 1);
}

}  // namespace
}  // namespace history_clusters
