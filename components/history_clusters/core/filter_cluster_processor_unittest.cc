// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/filter_cluster_processor.h"

#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "components/history_clusters/core/clustering_test_utils.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/history_clusters_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_clusters {
namespace {

using ::testing::ElementsAre;

std::vector<history::Cluster> GetTestClusters() {
  history::Cluster meets_no_criteria;
  meets_no_criteria.cluster_id = 1;

  history::Cluster meets_all_criteria;
  meets_all_criteria.cluster_id = 2;
  history::AnnotatedVisit visit =
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://github.com/"));
  visit.visit_row.is_known_to_sync = true;
  visit.content_annotations.has_url_keyed_image = true;
  visit.content_annotations.model_annotations.categories = {{"category1", 90},
                                                            {"category2", 84}};
  history::AnnotatedVisit visit2 =
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://search.com/"));
  visit2.content_annotations.search_terms = u"search";
  visit2.content_annotations.related_searches = {"relsearch1", "relsearch2"};
  history::AnnotatedVisit visit4 =
      testing::CreateDefaultAnnotatedVisit(4, GURL("https://github.com/2"));
  visit4.content_annotations.model_annotations.categories = {{"category1", 85},
                                                             {"category3", 82}};
  visit4.content_annotations.has_url_keyed_image = true;
  visit4.visit_row.is_known_to_sync = true;
  meets_all_criteria.visits = {testing::CreateClusterVisit(visit),
                               testing::CreateClusterVisit(visit2),
                               testing::CreateClusterVisit(visit4)};

  history::Cluster not_enough_images = meets_all_criteria;
  not_enough_images.cluster_id = 3;
  not_enough_images.visits[0]
      .annotated_visit.content_annotations.has_url_keyed_image = false;

  history::Cluster no_categories = meets_all_criteria;
  no_categories.cluster_id = 4;
  no_categories.visits[0]
      .annotated_visit.content_annotations.model_annotations.categories.clear();
  no_categories.visits[2]
      .annotated_visit.content_annotations.model_annotations.categories.clear();

  history::Cluster no_search_terms = meets_all_criteria;
  no_search_terms.cluster_id = 5;
  no_search_terms.visits[1].annotated_visit.content_annotations.search_terms =
      u"";

  history::Cluster no_related_searches = meets_all_criteria;
  no_related_searches.cluster_id = 6;
  no_related_searches.visits[1]
      .annotated_visit.content_annotations.related_searches.clear();

  history::Cluster noisy_cluster = meets_all_criteria;
  noisy_cluster.cluster_id = 7;
  for (auto& noisy_cluster_visit : noisy_cluster.visits) {
    noisy_cluster_visit.engagement_score =
        GetConfig().noisy_cluster_visits_engagement_threshold + 1;
    noisy_cluster_visit.annotated_visit.content_annotations.search_terms = u"";
  }

  history::Cluster single_visit_cluster = meets_all_criteria;
  single_visit_cluster.cluster_id = 8;
  single_visit_cluster.visits = {meets_all_criteria.visits[0]};

  history::Cluster non_visible_cluster = meets_all_criteria;
  non_visible_cluster.cluster_id = 9;
  non_visible_cluster.visits[0]
      .annotated_visit.content_annotations.model_annotations.visibility_score =
      GetConfig().content_visibility_threshold - 0.1;

  history::Cluster has_blocked_category = meets_all_criteria;
  has_blocked_category.cluster_id = 10;
  has_blocked_category.visits[0]
      .annotated_visit.content_annotations.model_annotations.categories
      .push_back({"blocked", 80});

  history::Cluster has_image_not_known_to_sync = meets_all_criteria;
  has_image_not_known_to_sync.cluster_id = 11;
  base::ranges::for_each(
      has_image_not_known_to_sync.visits, [&](auto& cluster_visit) {
        cluster_visit.annotated_visit.visit_row.is_known_to_sync = false;
      });

  history::Cluster meets_all_criteria_but_not_after_skipped_visits =
      meets_all_criteria;
  meets_all_criteria_but_not_after_skipped_visits.cluster_id = 12;
  base::ranges::for_each(
      meets_all_criteria_but_not_after_skipped_visits.visits,
      [&](auto& cluster_visit) { cluster_visit.score = 0.0; });

  return {meets_no_criteria,
          meets_all_criteria,
          not_enough_images,
          no_categories,
          no_search_terms,
          no_related_searches,
          noisy_cluster,
          single_visit_cluster,
          non_visible_cluster,
          has_blocked_category,
          has_image_not_known_to_sync,
          meets_all_criteria_but_not_after_skipped_visits};
}

class FilterClusterProcessorTest : public ::testing::Test {
 public:
  FilterClusterProcessorTest() = default;
  ~FilterClusterProcessorTest() override = default;

  // Runs the test clusters through a `FilterClusterProcessor` with
  // `filter_params`. Returns the ids of the clusters that remain.
  std::vector<int64_t> GetTestClusterIdsThatPassFilter(
      QueryClustersFilterParams& filter_params,
      bool engagement_score_provider_is_valid = true) {
    auto cluster_processor = std::make_unique<FilterClusterProcessor>(
        ClusteringRequestSource::kNewTabPage, filter_params,
        engagement_score_provider_is_valid);

    std::vector<history::Cluster> clusters = GetTestClusters();
    cluster_processor->ProcessClusters(&clusters);

    std::vector<int64_t> cluster_ids;
    cluster_ids.reserve(clusters.size());
    for (const auto& cluster : clusters) {
      cluster_ids.push_back(cluster.cluster_id);
    }
    return cluster_ids;
  }
};

TEST_F(FilterClusterProcessorTest,
       ShouldShowOnProminentUiSurfacesIsSetIfFilterParamsConditionIsSet) {
  QueryClustersFilterParams filter_params;
  filter_params.is_shown_on_prominent_ui_surfaces = true;

  auto cluster_processor = std::make_unique<FilterClusterProcessor>(
      ClusteringRequestSource::kNewTabPage, filter_params,
      /*engagement_score_provider_is_valid=*/true);

  std::vector<history::Cluster> clusters = GetTestClusters();
  base::ranges::for_each(clusters, [&](auto& cluster) {
    cluster.should_show_on_prominent_ui_surfaces = false;
  });
  cluster_processor->ProcessClusters(&clusters);

  // Some clusters are content visible - make sure there's at least one bit set
  // properly after culling non-prominent.
  std::erase_if(clusters, [](const history::Cluster& cluster) {
    return !cluster.should_show_on_prominent_ui_surfaces;
  });
  EXPECT_FALSE(clusters.empty());
}

TEST_F(FilterClusterProcessorTest, NoFunctionalFilter) {
  base::HistogramTester histogram_tester;

  QueryClustersFilterParams params;

  EXPECT_THAT(GetTestClusterIdsThatPassFilter(params),
              ElementsAre(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12));

  // Filter should not have been run, so expect these counts to be 0.
  histogram_tester.ExpectTotalCount(
      "History.Clusters.Backend.FilterClusterProcessor.NumClusters.PreFilter."
      "NewTabPage",
      0);
  histogram_tester.ExpectTotalCount(
      "History.Clusters.Backend.FilterClusterProcessor.NumClusters.PostFilter."
      "NewTabPage",
      0);
  histogram_tester.ExpectTotalCount(
      "History.Clusters.Backend.FilterClusterProcessor.ClusterFilterReason."
      "NewTabPage",
      0);
}

TEST_F(FilterClusterProcessorTest, OnlyVisitsConstraint) {
  base::HistogramTester histogram_tester;

  QueryClustersFilterParams params;
  params.min_visits = 2;

  EXPECT_THAT(GetTestClusterIdsThatPassFilter(params),
              ElementsAre(2, 3, 4, 5, 6, 7, 9, 10, 11));

  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.FilterClusterProcessor.NumClusters.PreFilter."
      "NewTabPage",
      12, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.FilterClusterProcessor.NumClusters.PostFilter."
      "NewTabPage",
      9, 1);
  histogram_tester.ExpectBucketCount(
      "History.Clusters.Backend.FilterClusterProcessor.ClusterFilterReason."
      "NewTabPage",
      ClusterFilterReason::kNotFiltered, 9);
  histogram_tester.ExpectBucketCount(
      "History.Clusters.Backend.FilterClusterProcessor.ClusterFilterReason."
      "NewTabPage",
      ClusterFilterReason::kNotEnoughVisits, 3);
}

TEST_F(FilterClusterProcessorTest, OnlyImageConstraint) {
  base::HistogramTester histogram_tester;

  QueryClustersFilterParams params;
  params.min_visits_with_images = 2;

  EXPECT_THAT(GetTestClusterIdsThatPassFilter(params),
              ElementsAre(2, 4, 5, 6, 7, 9, 10));

  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.FilterClusterProcessor.NumClusters.PreFilter."
      "NewTabPage",
      12, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.FilterClusterProcessor.NumClusters.PostFilter."
      "NewTabPage",
      7, 1);
  histogram_tester.ExpectBucketCount(
      "History.Clusters.Backend.FilterClusterProcessor.ClusterFilterReason."
      "NewTabPage",
      ClusterFilterReason::kNotFiltered, 7);
  histogram_tester.ExpectBucketCount(
      "History.Clusters.Backend.FilterClusterProcessor.ClusterFilterReason."
      "NewTabPage",
      ClusterFilterReason::kNotEnoughImages, 5);
}

TEST_F(FilterClusterProcessorTest, OnlyCategoryAllowlistConstraint) {
  base::HistogramTester histogram_tester;

  QueryClustersFilterParams params;
  params.categories_allowlist = {"category1", "category2"};

  EXPECT_THAT(GetTestClusterIdsThatPassFilter(params),
              ElementsAre(2, 3, 5, 6, 7, 8, 9, 10, 11));

  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.FilterClusterProcessor.NumClusters.PreFilter."
      "NewTabPage",
      12, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.FilterClusterProcessor.NumClusters.PostFilter."
      "NewTabPage",
      9, 1);
  histogram_tester.ExpectBucketCount(
      "History.Clusters.Backend.FilterClusterProcessor.ClusterFilterReason."
      "NewTabPage",
      ClusterFilterReason::kNotFiltered, 9);
  histogram_tester.ExpectBucketCount(
      "History.Clusters.Backend.FilterClusterProcessor.ClusterFilterReason."
      "NewTabPage",
      ClusterFilterReason::kNoCategoryMatch, 3);
}

TEST_F(FilterClusterProcessorTest, OnlyCategoryBlocklistConstraint) {
  base::HistogramTester histogram_tester;

  QueryClustersFilterParams params;
  params.categories_blocklist = {"blocked"};

  EXPECT_THAT(GetTestClusterIdsThatPassFilter(params),
              ElementsAre(1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12));

  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.FilterClusterProcessor.NumClusters.PreFilter."
      "NewTabPage",
      12, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.FilterClusterProcessor.NumClusters.PostFilter."
      "NewTabPage",
      11, 1);
  histogram_tester.ExpectBucketCount(
      "History.Clusters.Backend.FilterClusterProcessor.ClusterFilterReason."
      "NewTabPage",
      ClusterFilterReason::kNotFiltered, 11);
  histogram_tester.ExpectBucketCount(
      "History.Clusters.Backend.FilterClusterProcessor.ClusterFilterReason."
      "NewTabPage",
      ClusterFilterReason::kHasBlockedCategory, 1);
}

TEST_F(FilterClusterProcessorTest, OnlySearchInitiated) {
  base::HistogramTester histogram_tester;

  QueryClustersFilterParams params;
  params.is_search_initiated = true;

  EXPECT_THAT(GetTestClusterIdsThatPassFilter(params),
              ElementsAre(2, 3, 4, 6, 9, 10, 11));

  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.FilterClusterProcessor.NumClusters.PreFilter."
      "NewTabPage",
      12, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.FilterClusterProcessor.NumClusters.PostFilter."
      "NewTabPage",
      7, 1);
  histogram_tester.ExpectBucketCount(
      "History.Clusters.Backend.FilterClusterProcessor.ClusterFilterReason."
      "NewTabPage",
      ClusterFilterReason::kNotFiltered, 7);
  histogram_tester.ExpectBucketCount(
      "History.Clusters.Backend.FilterClusterProcessor.ClusterFilterReason."
      "NewTabPage",
      ClusterFilterReason::kNotSearchInitiated, 5);
}

TEST_F(FilterClusterProcessorTest, OnlyRelatedSearches) {
  base::HistogramTester histogram_tester;

  QueryClustersFilterParams params;
  params.has_related_searches = true;

  EXPECT_THAT(GetTestClusterIdsThatPassFilter(params),
              ElementsAre(2, 3, 4, 5, 7, 9, 10, 11));

  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.FilterClusterProcessor.NumClusters.PreFilter."
      "NewTabPage",
      12, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.FilterClusterProcessor.NumClusters.PostFilter."
      "NewTabPage",
      8, 1);
  histogram_tester.ExpectBucketCount(
      "History.Clusters.Backend.FilterClusterProcessor.ClusterFilterReason."
      "NewTabPage",
      ClusterFilterReason::kNotFiltered, 8);
  histogram_tester.ExpectBucketCount(
      "History.Clusters.Backend.FilterClusterProcessor.ClusterFilterReason."
      "NewTabPage",
      ClusterFilterReason::kNoRelatedSearches, 4);
}

TEST_F(FilterClusterProcessorTest, OnlyShownOnProminentUiSurfacesNoEngagement) {
  base::HistogramTester histogram_tester;

  QueryClustersFilterParams params;
  params.is_shown_on_prominent_ui_surfaces = true;

  EXPECT_THAT(GetTestClusterIdsThatPassFilter(
                  params, /*engagement_score_provider_is_valid=*/false),
              ElementsAre(2, 3, 4, 5, 6, 7, 10, 11));

  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.FilterClusterProcessor.NumClusters.PreFilter."
      "NewTabPage",
      12, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.FilterClusterProcessor.NumClusters.PostFilter."
      "NewTabPage",
      8, 1);
  histogram_tester.ExpectBucketCount(
      "History.Clusters.Backend.FilterClusterProcessor.ClusterFilterReason."
      "NewTabPage",
      ClusterFilterReason::kNotFiltered, 8);
  histogram_tester.ExpectBucketCount(
      "History.Clusters.Backend.FilterClusterProcessor.ClusterFilterReason."
      "NewTabPage",
      ClusterFilterReason::kNotEnoughInterestingVisits, 0);
  histogram_tester.ExpectBucketCount(
      "History.Clusters.Backend.FilterClusterProcessor.ClusterFilterReason."
      "NewTabPage",
      ClusterFilterReason::kSingleVisit, 3);
  histogram_tester.ExpectBucketCount(
      "History.Clusters.Backend.FilterClusterProcessor.ClusterFilterReason."
      "NewTabPage",
      ClusterFilterReason::kNotContentVisible, 1);
}

TEST_F(FilterClusterProcessorTest,
       OnlyShownOnProminentUiSurfacesWithEngagement) {
  base::HistogramTester histogram_tester;

  QueryClustersFilterParams params;
  params.is_shown_on_prominent_ui_surfaces = true;

  EXPECT_THAT(GetTestClusterIdsThatPassFilter(
                  params, /*engagement_score_provider_is_valid=*/true),
              ElementsAre(2, 3, 4, 5, 6, 10, 11));

  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.FilterClusterProcessor.NumClusters.PreFilter."
      "NewTabPage",
      12, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.FilterClusterProcessor.NumClusters.PostFilter."
      "NewTabPage",
      7, 1);
  histogram_tester.ExpectBucketCount(
      "History.Clusters.Backend.FilterClusterProcessor.ClusterFilterReason."
      "NewTabPage",
      ClusterFilterReason::kNotFiltered, 7);
  histogram_tester.ExpectBucketCount(
      "History.Clusters.Backend.FilterClusterProcessor.ClusterFilterReason."
      "NewTabPage",
      ClusterFilterReason::kNotEnoughInterestingVisits, 3);
  histogram_tester.ExpectBucketCount(
      "History.Clusters.Backend.FilterClusterProcessor.ClusterFilterReason."
      "NewTabPage",
      ClusterFilterReason::kSingleVisit, 3);
  histogram_tester.ExpectBucketCount(
      "History.Clusters.Backend.FilterClusterProcessor.ClusterFilterReason."
      "NewTabPage",
      ClusterFilterReason::kNotContentVisible, 1);
}

TEST_F(FilterClusterProcessorTest, FullFilter) {
  base::HistogramTester histogram_tester;

  QueryClustersFilterParams params;
  params.min_visits = 2;
  params.min_visits_with_images = 2;
  params.categories_allowlist = {"category1", "category2"};
  params.categories_blocklist = {"blocked"};
  params.is_search_initiated = true;
  params.has_related_searches = true;
  params.is_shown_on_prominent_ui_surfaces = true;

  EXPECT_THAT(GetTestClusterIdsThatPassFilter(params), ElementsAre(2));

  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.FilterClusterProcessor.NumClusters.PreFilter."
      "NewTabPage",
      12, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Clusters.Backend.FilterClusterProcessor.NumClusters.PostFilter."
      "NewTabPage",
      1, 1);
  histogram_tester.ExpectBucketCount(
      "History.Clusters.Backend.FilterClusterProcessor.ClusterFilterReason."
      "NewTabPage",
      ClusterFilterReason::kNotFiltered, 1);
  histogram_tester.ExpectBucketCount(
      "History.Clusters.Backend.FilterClusterProcessor.ClusterFilterReason."
      "NewTabPage",
      ClusterFilterReason::kNotEnoughVisits, 3);
  histogram_tester.ExpectBucketCount(
      "History.Clusters.Backend.FilterClusterProcessor.ClusterFilterReason."
      "NewTabPage",
      ClusterFilterReason::kNotEnoughImages, 5);
  histogram_tester.ExpectBucketCount(
      "History.Clusters.Backend.FilterClusterProcessor.ClusterFilterReason."
      "NewTabPage",
      ClusterFilterReason::kNoCategoryMatch, 3);
  histogram_tester.ExpectBucketCount(
      "History.Clusters.Backend.FilterClusterProcessor.ClusterFilterReason."
      "NewTabPage",
      ClusterFilterReason::kNotSearchInitiated, 5);
  histogram_tester.ExpectBucketCount(
      "History.Clusters.Backend.FilterClusterProcessor.ClusterFilterReason."
      "NewTabPage",
      ClusterFilterReason::kNoRelatedSearches, 4);
  histogram_tester.ExpectBucketCount(
      "History.Clusters.Backend.FilterClusterProcessor.ClusterFilterReason."
      "NewTabPage",
      ClusterFilterReason::kNotEnoughInterestingVisits, 3);
  histogram_tester.ExpectBucketCount(
      "History.Clusters.Backend.FilterClusterProcessor.ClusterFilterReason."
      "NewTabPage",
      ClusterFilterReason::kSingleVisit, 3);
  histogram_tester.ExpectBucketCount(
      "History.Clusters.Backend.FilterClusterProcessor.ClusterFilterReason."
      "NewTabPage",
      ClusterFilterReason::kNotContentVisible, 1);
  histogram_tester.ExpectBucketCount(
      "History.Clusters.Backend.FilterClusterProcessor.ClusterFilterReason."
      "NewTabPage",
      ClusterFilterReason::kHasBlockedCategory, 1);
}

}  // namespace
}  // namespace history_clusters
