// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/on_device_clustering_util.h"

#include "base/test/task_environment.h"
#include "components/history/core/browser/url_row.h"
#include "components/history_clusters/core/clustering_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_clusters {
namespace {

using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

using OnDeviceClusteringUtilTest = ::testing::Test;

TEST_F(OnDeviceClusteringUtilTest, MergeDuplicateVisitIntoCanonicalVisit) {
  // canonical_visit has the same normalized URL as duplicated_visit.
  history::ClusterVisit duplicate_visit = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(
          1, GURL("https://example.com/normalized?q=whatever")),
      GURL("https://example.com/normalized"));
  duplicate_visit.annotated_visit.content_annotations.related_searches = {
      "xyz"};
  duplicate_visit.annotated_visit.context_annotations.omnibox_url_copied = true;
  duplicate_visit.annotated_visit.context_annotations.is_existing_bookmark =
      true;
  duplicate_visit.annotated_visit.context_annotations
      .is_existing_part_of_tab_group = true;
  duplicate_visit.annotated_visit.context_annotations.is_new_bookmark = true;
  duplicate_visit.annotated_visit.context_annotations.is_placed_in_tab_group =
      true;
  duplicate_visit.annotated_visit.context_annotations.is_ntp_custom_link = true;
  duplicate_visit.annotated_visit.context_annotations
      .total_foreground_duration = base::Seconds(20);

  duplicate_visit.annotated_visit.content_annotations.model_annotations
      .visibility_score = 0.6;
  duplicate_visit.annotated_visit.content_annotations.model_annotations
      .categories.emplace_back("category1", 40);
  duplicate_visit.annotated_visit.content_annotations.model_annotations.entities
      .emplace_back("entity1", 20);

  history::ClusterVisit canonical_visit =
      testing::CreateClusterVisit(testing::CreateDefaultAnnotatedVisit(
          2, GURL("https://example.com/normalized")));
  canonical_visit.annotated_visit.content_annotations.related_searches = {
      "abc", "xyz"};
  canonical_visit.annotated_visit.context_annotations.omnibox_url_copied =
      false;
  canonical_visit.annotated_visit.context_annotations.is_existing_bookmark =
      false;
  canonical_visit.annotated_visit.context_annotations
      .is_existing_part_of_tab_group = false;
  canonical_visit.annotated_visit.context_annotations.is_new_bookmark = false;
  canonical_visit.annotated_visit.context_annotations.is_placed_in_tab_group =
      false;
  canonical_visit.annotated_visit.context_annotations.is_ntp_custom_link =
      false;
  canonical_visit.annotated_visit.context_annotations
      .total_foreground_duration = base::Seconds(20);
  canonical_visit.annotated_visit.content_annotations.model_annotations
      .visibility_score = 0.5;
  canonical_visit.annotated_visit.content_annotations.model_annotations
      .categories.emplace_back("category1", 20);

  MergeDuplicateVisitIntoCanonicalVisit(std::move(duplicate_visit),
                                        canonical_visit);
  EXPECT_TRUE(
      canonical_visit.annotated_visit.context_annotations.omnibox_url_copied);
  EXPECT_TRUE(
      canonical_visit.annotated_visit.context_annotations.is_existing_bookmark);
  EXPECT_TRUE(canonical_visit.annotated_visit.context_annotations
                  .is_existing_part_of_tab_group);
  EXPECT_TRUE(
      canonical_visit.annotated_visit.context_annotations.is_new_bookmark);
  EXPECT_TRUE(canonical_visit.annotated_visit.context_annotations
                  .is_placed_in_tab_group);
  EXPECT_TRUE(
      canonical_visit.annotated_visit.context_annotations.is_ntp_custom_link);
  EXPECT_THAT(
      canonical_visit.annotated_visit.content_annotations.related_searches,
      UnorderedElementsAre("abc", "xyz"));
  EXPECT_EQ(canonical_visit.annotated_visit.visit_row.visit_duration,
            base::Seconds(10 * 2));
  EXPECT_EQ(canonical_visit.annotated_visit.context_annotations
                .total_foreground_duration,
            base::Seconds(20 * 2));

  EXPECT_FLOAT_EQ(canonical_visit.annotated_visit.content_annotations
                      .model_annotations.visibility_score,
                  0.5);

  ASSERT_EQ(canonical_visit.annotated_visit.content_annotations
                .model_annotations.categories.size(),
            1U);
  EXPECT_EQ(
      canonical_visit.annotated_visit.content_annotations.model_annotations
          .categories[0]
          .id,
      "category1");
  EXPECT_EQ(
      canonical_visit.annotated_visit.content_annotations.model_annotations
          .categories[0]
          .weight,
      40);

  ASSERT_EQ(canonical_visit.annotated_visit.content_annotations
                .model_annotations.entities.size(),
            1U);
  EXPECT_EQ(
      canonical_visit.annotated_visit.content_annotations.model_annotations
          .entities[0]
          .id,
      "entity1");
  EXPECT_EQ(
      canonical_visit.annotated_visit.content_annotations.model_annotations
          .entities[0]
          .weight,
      20);

  EXPECT_FLOAT_EQ(canonical_visit.score, 1.0f);
}

TEST_F(OnDeviceClusteringUtilTest,
       MergeDuplicateVisitIntoCanonicalVisitMaintainsZeroScore) {
  // canonical_visit has the same normalized URL as duplicated_visit.
  history::ClusterVisit duplicate_visit = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(
          1, GURL("https://example.com/normalized?q=whatever")),
      GURL("https://example.com/normalized"));
  duplicate_visit.score = 0.0;

  history::ClusterVisit canonical_visit =
      testing::CreateClusterVisit(testing::CreateDefaultAnnotatedVisit(
          2, GURL("https://example.com/normalized")));

  MergeDuplicateVisitIntoCanonicalVisit(std::move(duplicate_visit),
                                        canonical_visit);

  EXPECT_FLOAT_EQ(canonical_visit.score, 0.0f);
}

TEST_F(OnDeviceClusteringUtilTest, IsNoisyVisitSearchHighEngagementVisit) {
  history::ClusterVisit visit;
  visit.annotated_visit.content_annotations.search_terms = u"search";
  visit.engagement_score = 90.0;
  EXPECT_FALSE(IsNoisyVisit(visit));
}

TEST_F(OnDeviceClusteringUtilTest, IsNoisyVisitNotSearchHighEngagementVisit) {
  history::ClusterVisit visit;
  visit.annotated_visit.content_annotations.search_terms = u"";
  visit.engagement_score = 90.0;
  EXPECT_TRUE(IsNoisyVisit(visit));
}

TEST_F(OnDeviceClusteringUtilTest, IsNoisyVisitNotSearchLowEngagementVisit) {
  history::ClusterVisit visit;
  visit.annotated_visit.content_annotations.search_terms = u"";
  visit.engagement_score = 1.0;
  EXPECT_FALSE(IsNoisyVisit(visit));
}

TEST_F(OnDeviceClusteringUtilTest, IsNoisyVisitSearchLowEngagementVisit) {
  history::ClusterVisit visit;
  visit.annotated_visit.content_annotations.search_terms = u"search";
  visit.engagement_score = 1.0;
  EXPECT_FALSE(IsNoisyVisit(visit));
}

TEST_F(OnDeviceClusteringUtilTest, AppendClusterVisits) {
  history::Cluster cluster1 = history::Cluster(
      0,
      {
          testing::CreateClusterVisit(
              testing::CreateDefaultAnnotatedVisit(1, GURL("https://two.com/"),
                                                   base::Time::FromTimeT(10)),
              std::nullopt, 0.1),
      },
      {});

  history::Cluster cluster2 = history::Cluster(
      0,
      {
          testing::CreateClusterVisit(
              testing::CreateDefaultAnnotatedVisit(2, GURL("https://two.com/"),
                                                   base::Time::FromTimeT(10)),
              std::nullopt, 0.1),
      },
      {});

  AppendClusterVisits(cluster1, cluster2);

  ASSERT_THAT(cluster1.visits.size(), 2u);
  ASSERT_TRUE(cluster2.visits.empty());
  EXPECT_THAT(testing::ToVisitResults({cluster1}),
              ElementsAre(ElementsAre(testing::VisitResult(1, 0.1),
                                      testing::VisitResult(2, 0.1))));
}

TEST_F(OnDeviceClusteringUtilTest, RemoveEmptyClusters) {
  std::vector<history::Cluster> clusters;
  clusters.push_back(history::Cluster(
      0,
      {
          testing::CreateClusterVisit(
              testing::CreateDefaultAnnotatedVisit(2, GURL("https://two.com/"),
                                                   base::Time::FromTimeT(10)),
              std::nullopt, 0.1),
      },
      {}));

  clusters.push_back(history::Cluster(0, {}, {}));

  RemoveEmptyClusters(&clusters);

  ASSERT_THAT(clusters.size(), 1u);
  EXPECT_THAT(clusters[0].visits.size(), 1u);
}

}  // namespace
}  // namespace history_clusters
