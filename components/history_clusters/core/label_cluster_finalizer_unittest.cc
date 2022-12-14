// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/label_cluster_finalizer.h"

#include "base/test/task_environment.h"
#include "components/history_clusters/core/clustering_test_utils.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/on_device_clustering_features.h"
#include "components/optimization_guide/core/entity_metadata.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_clusters {
namespace {

using ::testing::UnorderedElementsAre;
using LabelSource = history::Cluster::LabelSource;

class LabelClusterFinalizerTest : public ::testing::Test {
 public:
  void SetUp() override {
    optimization_guide::EntityMetadata md1;
    md1.human_readable_name = "doesntmatter";
    entity_metadata_map_["someotherentity"] = md1;
    optimization_guide::EntityMetadata md2;
    md2.human_readable_name = "doesntmatter";
    entity_metadata_map_["highscoringentitybutlowvisitscore"] = md2;
    optimization_guide::EntityMetadata label_md;
    label_md.human_readable_name = "chosenlabel";
    entity_metadata_map_["baz"] = label_md;

    cluster_finalizer_ =
        std::make_unique<LabelClusterFinalizer>(&entity_metadata_map_);
  }

  void TearDown() override { cluster_finalizer_.reset(); }

  void FinalizeCluster(history::Cluster& cluster) {
    cluster_finalizer_->FinalizeCluster(cluster);
  }

 private:
  base::flat_map<std::string, optimization_guide::EntityMetadata>
      entity_metadata_map_;
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
    // With only search term labelling active, there should be no label.
    Config config;
    config.labels_from_hostnames = false;
    config.labels_from_entities = false;
    SetConfigForTesting(config);

    history::Cluster cluster;
    cluster.visits = {visit2, visit3};
    FinalizeCluster(cluster);
    EXPECT_EQ(cluster.raw_label, absl::nullopt);
    EXPECT_EQ(cluster.label, absl::nullopt);
  }

  {
    // With hostname labelling and entity labelling both enabled, we should
    // prefer the entity because if we prefer hostnames, every cluster will have
    // a hostname label, and no entity labels will ever get surfaced.
    Config config;
    config.labels_from_hostnames = true;
    config.labels_from_entities = true;
    SetConfigForTesting(config);

    history::Cluster cluster;
    cluster.visits = {visit2, visit3};
    FinalizeCluster(cluster);
    EXPECT_EQ(cluster.raw_label, u"chosenlabel");
    EXPECT_EQ(cluster.label, u"chosenlabel");
  }

  {
    // With hostname labelling active only, we should use the hostname.
    Config config;
    config.labels_from_hostnames = true;
    config.labels_from_entities = false;
    SetConfigForTesting(config);

    history::Cluster cluster;
    cluster.visits = {visit2, visit3};
    FinalizeCluster(cluster);
    EXPECT_EQ(cluster.raw_label, u"baz.com");
    EXPECT_EQ(cluster.label, u"baz.com and more");
  }

  {
    // With entity labelling active only, we should use the entity name.
    Config config;
    config.labels_from_hostnames = false;
    config.labels_from_entities = true;
    SetConfigForTesting(config);

    history::Cluster cluster;
    cluster.visits = {visit2, visit3};
    FinalizeCluster(cluster);
    EXPECT_EQ(cluster.raw_label, u"chosenlabel");
    EXPECT_EQ(cluster.label, u"chosenlabel");
  }
}

TEST_F(LabelClusterFinalizerTest, TakesHighestScoringSearchTermIfAvailable) {
  // Verify that search terms take precedence even if labels from entities are
  // enabled.
  Config config;
  config.labels_from_hostnames = true;
  config.labels_from_entities = true;
  SetConfigForTesting(config);

  history::ClusterVisit visit =
      testing::CreateClusterVisit(testing::CreateDefaultAnnotatedVisit(
          2, GURL("https://nosearchtermsbuthighscorevisit.com/")));
  visit.engagement_score = 0.9;
  visit.annotated_visit.content_annotations.model_annotations.entities = {
      {"github", 100}, {"onlyinnoisyvisit", 99}};

  history::ClusterVisit visit2 =
      testing::CreateClusterVisit(testing::CreateDefaultAnnotatedVisit(
          1, GURL("https://lowerscoringsearchterm.com/")));
  visit2.score = 0.6;
  visit2.annotated_visit.content_annotations.search_terms = u"lowscore";

  history::ClusterVisit visit3 = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://baz.com/")));
  visit3.score = 0.8;
  visit3.annotated_visit.content_annotations.model_annotations.entities = {
      {"github", 100}, {"someotherentity", 100}};
  visit3.annotated_visit.content_annotations.search_terms = u"searchtermlabel";

  history::Cluster cluster;
  cluster.visits = {visit, visit2, visit3};
  FinalizeCluster(cluster);
  EXPECT_THAT(cluster.raw_label, u"searchtermlabel");
  EXPECT_THAT(cluster.label, u"“searchtermlabel”");
}

}  // namespace
}  // namespace history_clusters
