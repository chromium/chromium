// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/keyword_cluster_finalizer.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/history_clusters/core/clustering_test_utils.h"
#include "components/history_clusters/core/on_device_clustering_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_clusters {
namespace {

using ::testing::UnorderedElementsAre;

class KeywordClusterFinalizerTest : public ::testing::Test {
 public:
  void SetUp() override {
    cluster_finalizer_ = std::make_unique<KeywordClusterFinalizer>();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kOnDeviceClustering,
        {{"exclude_keywords_from_noisy_visits", "true"},
         {"include_categories_in_keywords", "false"}});
  }

  void TearDown() override { cluster_finalizer_.reset(); }

  void FinalizeCluster(history::Cluster& cluster) {
    cluster_finalizer_->FinalizeCluster(cluster);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<KeywordClusterFinalizer> cluster_finalizer_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(KeywordClusterFinalizerTest, IncludesKeywordsBasedOnFeatureParameters) {
  history::ClusterVisit visit = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://foo.com/")));
  visit.engagement_score = 1.0;
  visit.annotated_visit.content_annotations.model_annotations.entities = {
      {"github", 1}};
  visit.annotated_visit.content_annotations.model_annotations.categories = {
      {"category", 1}};

  history::ClusterVisit visit2 =
      testing::CreateClusterVisit(testing::CreateDefaultAnnotatedVisit(
          2, GURL("https://engagementtoohigh.com/")));
  visit2.engagement_score = 25.0;
  visit2.annotated_visit.content_annotations.model_annotations.entities = {
      {"github", 1}, {"onlyinnoisyvisit", 1}};
  visit2.annotated_visit.content_annotations.model_annotations.categories = {
      {"category", 1}};

  history::ClusterVisit visit3 = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://baz.com/")));
  visit3.duplicate_visit_ids.push_back(1);
  visit3.engagement_score = 1.0;
  visit3.annotated_visit.content_annotations.model_annotations.entities = {
      {"github", 1}, {"otherentity", 1}};
  visit3.annotated_visit.content_annotations.model_annotations.categories = {
      {"category", 1}};

  history::Cluster cluster;
  cluster.visits = {visit, visit2, visit3};
  FinalizeCluster(cluster);
  EXPECT_THAT(cluster.keywords,
              UnorderedElementsAre(u"github", u"otherentity"));
}

class KeywordClusterFinalizerIncludeAllTest : public ::testing::Test {
 public:
  void SetUp() override {
    cluster_finalizer_ = std::make_unique<KeywordClusterFinalizer>();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kOnDeviceClustering,
        {{"exclude_keywords_from_noisy_visits", "false"},
         {"include_categories_in_keywords", "true"}});
  }

  void TearDown() override { cluster_finalizer_.reset(); }

  void FinalizeCluster(history::Cluster& cluster) {
    cluster_finalizer_->FinalizeCluster(cluster);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<KeywordClusterFinalizer> cluster_finalizer_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(KeywordClusterFinalizerIncludeAllTest,
       IncludesKeywordsBasedOnFeatureParameters) {
  history::ClusterVisit visit = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://foo.com/")));
  visit.engagement_score = 1.0;
  visit.annotated_visit.content_annotations.model_annotations.entities = {
      {"github", 1}};
  visit.annotated_visit.content_annotations.model_annotations.categories = {
      {"category", 1}};

  history::ClusterVisit visit2 =
      testing::CreateClusterVisit(testing::CreateDefaultAnnotatedVisit(
          2, GURL("https://engagementtoohigh.com/")));
  visit2.engagement_score = 25.0;
  visit2.annotated_visit.content_annotations.model_annotations.entities = {
      {"github", 1}, {"onlyinnoisyvisit", 1}};
  visit2.annotated_visit.content_annotations.model_annotations.categories = {
      {"category", 1}};

  history::ClusterVisit visit3 = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://baz.com/")));
  visit3.duplicate_visit_ids.push_back(1);
  visit3.engagement_score = 1.0;
  visit3.annotated_visit.content_annotations.model_annotations.entities = {
      {"github", 1}, {"otherentity", 1}};
  visit3.annotated_visit.content_annotations.model_annotations.categories = {
      {"category", 1}};

  history::Cluster cluster;
  cluster.visits = {visit, visit2, visit3};
  FinalizeCluster(cluster);
  EXPECT_THAT(cluster.keywords,
              UnorderedElementsAre(u"github", u"category", u"onlyinnoisyvisit",
                                   u"otherentity"));
}

}  // namespace
}  // namespace history_clusters
