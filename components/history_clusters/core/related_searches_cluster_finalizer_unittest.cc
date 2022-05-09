// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/related_searches_cluster_finalizer.h"

#include "components/history_clusters/core/clustering_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_clusters {
namespace {

using ::testing::ElementsAre;

class RelatedSearchesClusterFinalizerTest : public ::testing::Test {
 public:
  void SetUp() override {
    cluster_finalizer_ = std::make_unique<RelatedSearchesClusterFinalizer>();
  }

  void TearDown() override { cluster_finalizer_.reset(); }

  void FinalizeCluster(history::Cluster& cluster) {
    cluster_finalizer_->FinalizeCluster(cluster);
  }

 private:
  std::unique_ptr<RelatedSearchesClusterFinalizer> cluster_finalizer_;
};

TEST_F(RelatedSearchesClusterFinalizerTest, DedupeExactURL) {
  // canonical_visit has the same URL as Visit1.
  history::ClusterVisit visit1 = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://google.com/")));
  visit1.annotated_visit.content_annotations.related_searches.push_back(
      "search1");
  visit1.annotated_visit.content_annotations.related_searches.push_back(
      "search2");
  visit1.annotated_visit.content_annotations.related_searches.push_back(
      "search3");

  history::ClusterVisit visit2 = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://google.com/")));
  visit2.annotated_visit.content_annotations.related_searches.push_back(
      "search4");
  visit2.annotated_visit.content_annotations.related_searches.push_back(
      "search5");
  visit2.annotated_visit.content_annotations.related_searches.push_back(
      "search6");

  history::Cluster cluster;
  cluster.visits = {visit1, visit2};
  FinalizeCluster(cluster);
  EXPECT_THAT(
      cluster.related_searches,
      ElementsAre("search1", "search2", "search3", "search4", "search5"));
}

}  // namespace
}  // namespace history_clusters
