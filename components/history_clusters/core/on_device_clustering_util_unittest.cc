// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/on_device_clustering_util.h"

#include "base/test/task_environment.h"
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

  MergeDuplicateVisitIntoCanonicalVisit(duplicate_visit, canonical_visit);
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
}

TEST_F(OnDeviceClusteringUtilTest, CalculateAllDuplicateVisitsForCluster) {
  history::ClusterVisit visit = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://google.com/")));
  history::ClusterVisit visit2 = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://foo.com/")));
  visit2.duplicate_visit_ids = {1};

  history::Cluster cluster;
  cluster.visits = {visit, visit2};

  EXPECT_THAT(CalculateAllDuplicateVisitsForCluster(cluster),
              UnorderedElementsAre(1));
}

TEST_F(OnDeviceClusteringUtilTest, IsNoisyVisitSearchHighEngagementVisit) {
  history::ClusterVisit visit;
  visit.is_search_visit = true;
  visit.engagement_score = 90.0;
  EXPECT_FALSE(IsNoisyVisit(visit));
}

TEST_F(OnDeviceClusteringUtilTest, IsNoisyVisitNotSearchHighEngagementVisit) {
  history::ClusterVisit visit;
  visit.is_search_visit = false;
  visit.engagement_score = 90.0;
  EXPECT_TRUE(IsNoisyVisit(visit));
}

TEST_F(OnDeviceClusteringUtilTest, IsNoisyVisitNotSearchLowEngagementVisit) {
  history::ClusterVisit visit;
  visit.is_search_visit = false;
  visit.engagement_score = 1.0;
  EXPECT_FALSE(IsNoisyVisit(visit));
}

TEST_F(OnDeviceClusteringUtilTest, IsNoisyVisitSearchLowEngagementVisit) {
  history::ClusterVisit visit;
  visit.is_search_visit = true;
  visit.engagement_score = 1.0;
  EXPECT_FALSE(IsNoisyVisit(visit));
}

}  // namespace
}  // namespace history_clusters
