// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/keyword_cluster_finalizer.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/history_clusters/core/clustering_test_utils.h"
#include "components/history_clusters/core/config.h"
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

    config_.keyword_filter_on_noisy_visits = false;
    SetConfigForTesting(config_);
  }

  void TearDown() override { cluster_finalizer_.reset(); }

  void FinalizeCluster(history::Cluster& cluster) {
    cluster_finalizer_->FinalizeCluster(cluster);
  }

 private:
  base::test::TaskEnvironment task_environment_;
  Config config_;
  std::unique_ptr<KeywordClusterFinalizer> cluster_finalizer_;
};

TEST_F(KeywordClusterFinalizerTest, IncludesKeywordsBasedOnFeatureParameters) {
  history::ClusterVisit visit = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://foo.com/")));
  visit.engagement_score = 1.0;
  visit.annotated_visit.content_annotations.model_annotations.entities = {
      {"github", 1}};
  visit.annotated_visit.content_annotations.model_annotations.categories = {
      {"category", 1}};
  visit.annotated_visit.content_annotations.search_terms = u"search";

  history::ClusterVisit visit2 =
      testing::CreateClusterVisit(testing::CreateDefaultAnnotatedVisit(
          2, GURL("https://engagementtoohigh.com/")));
  visit2.engagement_score = 25.0;
  visit2.annotated_visit.content_annotations.model_annotations.entities = {
      {"github", 1}, {"onlyinnoisyvisit", 1}};
  visit2.annotated_visit.content_annotations.model_annotations.categories = {
      {"category", 1}};

  history::ClusterVisit visit3 = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(3, GURL("https://baz.com/")));
  visit3.duplicate_visits.push_back(
      testing::ClusterVisitToDuplicateClusterVisit(visit));
  visit3.engagement_score = 1.0;
  visit3.annotated_visit.content_annotations.model_annotations.entities = {
      {"github", 1}, {"otherentity", 1}};
  visit3.annotated_visit.content_annotations.model_annotations.categories = {
      {"category", 1}};
  visit3.annotated_visit.content_annotations.search_terms = u"search";

  history::Cluster cluster;
  cluster.visits = {visit2, visit3};
  FinalizeCluster(cluster);

  EXPECT_THAT(cluster.GetKeywords(), UnorderedElementsAre(u"search"));
  EXPECT_EQ(cluster.keyword_to_data_map.at(u"search"),
            history::ClusterKeywordData(
                history::ClusterKeywordData::kSearchTerms, 100));
}

}  // namespace
}  // namespace history_clusters
