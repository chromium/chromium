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
}

TEST_F(OnDeviceClusteringUtilTest, SortClusters) {
  std::vector<history::Cluster> clusters;
  // This first cluster is meant to validate that the higher scoring "visit 1"
  // gets sorted to the top, even though "visit 1" is older than "visit 2".
  // It's to validate the within-cluster sorting.
  clusters.push_back(history::Cluster(
      0,
      {
          testing::CreateClusterVisit(
              testing::CreateDefaultAnnotatedVisit(2, GURL("https://two.com/"),
                                                   base::Time::FromTimeT(10)),
              absl::nullopt, 0.5),
          testing::CreateClusterVisit(
              testing::CreateDefaultAnnotatedVisit(1, GURL("https://one.com/"),
                                                   base::Time::FromTimeT(5)),
              absl::nullopt, 0.9),
      },
      {}));
  // The second cluster is lower scoring, but newer, because the top visit is
  // newer. It should be sorted above the first cluster because of reverse
  // chronological between-cluster sorting.
  clusters.push_back(history::Cluster(
      0,
      {
          testing::CreateClusterVisit(
              testing::CreateDefaultAnnotatedVisit(2, GURL("https://two.com/"),
                                                   base::Time::FromTimeT(10)),
              absl::nullopt, 0.1),
      },
      {}));

  SortClusters(&clusters);

  ASSERT_EQ(clusters.size(), 2u);

  auto& visits = clusters[0].visits;
  ASSERT_EQ(visits.size(), 1u);
  EXPECT_EQ(visits[0].annotated_visit.url_row.url(), "https://two.com/");
  EXPECT_FLOAT_EQ(visits[0].score, 0.1);

  visits = clusters[1].visits;
  ASSERT_EQ(visits.size(), 2u);
  EXPECT_EQ(visits[0].annotated_visit.url_row.url(), "https://one.com/");
  EXPECT_FLOAT_EQ(visits[0].score, 0.9);
  EXPECT_EQ(visits[1].annotated_visit.url_row.url(), "https://two.com/");
  EXPECT_FLOAT_EQ(visits[1].score, 0.5);
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

}  // namespace
}  // namespace history_clusters
