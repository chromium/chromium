// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/on_device_clustering_features.h"

#include <algorithm>
#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"

namespace history_clusters {
namespace features {

const base::Feature kOnDeviceClustering{"HistoryClustersOnDeviceClustering",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kUseEngagementScoreCache{"JourneysUseEngagementScoreCache",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSplitClusteringTasksToSmallerBatches{
    "JourneysSplitClusteringTasksToSmallerBatches",
    base::FEATURE_ENABLED_BY_DEFAULT};

base::TimeDelta ClusterNavigationTimeCutoff() {
  return base::Minutes(GetFieldTrialParamByFeatureAsInt(
      kOnDeviceClustering, "navigation_time_cutoff_minutes", 60));
}

bool ContentClusteringEnabled() {
  return GetFieldTrialParamByFeatureAsBool(kOnDeviceClustering,
                                           "content_clustering_enabled", true);
}

float ContentClusteringEntitySimilarityWeight() {
  return GetFieldTrialParamByFeatureAsDouble(
      kOnDeviceClustering, "content_clustering_entity_similarity_weight", 1.0);
}

float ContentClusteringCategorySimilarityWeight() {
  return GetFieldTrialParamByFeatureAsDouble(
      kOnDeviceClustering, "content_clustering_category_similarity_weight",
      1.0);
}

float ContentClusteringSimilarityThreshold() {
  float threshold = GetFieldTrialParamByFeatureAsDouble(
      kOnDeviceClustering, "content_clustering_similarity_threshold", 0.2);
  // Ensure that the value is [0.0 and 1.0].
  return std::max(0.0f, std::min(1.0f, threshold));
}

float ContentVisibilityThreshold() {
  float threshold = GetFieldTrialParamByFeatureAsDouble(
      kOnDeviceClustering, "content_visibility_threshold", 0.7);
  // Ensure that the value is [0.0 and 1.0].
  return std::max(0.0f, std::min(1.0f, threshold));
}

int64_t GetMinPageTopicsModelVersionToUseContentVisibilityFrom() {
  std::string value_as_string = GetFieldTrialParamValueByFeature(
      kOnDeviceClustering, "min_page_topics_model_version_for_visibility");
  int64_t value_as_int = 0;
  if (!base::StringToInt64(value_as_string, &value_as_int)) {
    value_as_int = INT64_MAX;
  }
  return value_as_int;
}

bool ShouldHideSingleVisitClustersOnProminentUISurfaces() {
  return GetFieldTrialParamByFeatureAsBool(
      kOnDeviceClustering,
      "hide_single_visit_clusters_on_prominent_ui_surfaces", true);
}

bool ShouldDedupeSimilarVisits() {
  return GetFieldTrialParamByFeatureAsBool(kOnDeviceClustering,
                                           "dedupe_similar_visits", true);
}

bool ShouldFilterNoisyClusters() {
  return GetFieldTrialParamByFeatureAsBool(kOnDeviceClustering,
                                           "filter_noisy_clusters", true);
}

float NoisyClusterVisitEngagementThreshold() {
  float threshold = GetFieldTrialParamByFeatureAsDouble(
      kOnDeviceClustering, "noisy_cluster_visit_engagement_threshold", 15.0);
  return threshold;
}

size_t NumberInterestingVisitsFilterThreshold() {
  int threshold = GetFieldTrialParamByFeatureAsInt(
      kOnDeviceClustering, "num_interesting_visits_filter_threshold", 1);
  return threshold;
}

float VisitDurationRankingWeight() {
  float weight = GetFieldTrialParamByFeatureAsDouble(
      kOnDeviceClustering, "visit_duration_ranking_weight", 1.0);
  return std::max(0.f, weight);
}

float ForegroundDurationRankingWeight() {
  float weight = GetFieldTrialParamByFeatureAsDouble(
      kOnDeviceClustering, "foreground_duration_ranking_weight", 1.5);
  return std::max(0.f, weight);
}

float BookmarkRankingWeight() {
  float weight = GetFieldTrialParamByFeatureAsDouble(
      kOnDeviceClustering, "bookmark_ranking_weight", 1.0);
  return std::max(0.f, weight);
}

float SearchResultsPageRankingWeight() {
  float weight = GetFieldTrialParamByFeatureAsDouble(
      kOnDeviceClustering, "search_results_page_ranking_weight", 2.0);
  return std::max(0.f, weight);
}

float HasPageTitleRankingWeight() {
  float weight = GetFieldTrialParamByFeatureAsDouble(
      kOnDeviceClustering, "has_page_title_ranking_weight", 2.0);
  return std::max(0.f, weight);
}

bool ContentClusterOnIntersectionSimilarity() {
  return GetFieldTrialParamByFeatureAsBool(
      kOnDeviceClustering, "use_content_clustering_intersection_similarity",
      true);
}

int ClusterIntersectionThreshold() {
  return GetFieldTrialParamByFeatureAsInt(
      kOnDeviceClustering, "content_clustering_intersection_threshold", 2);
}

bool ShouldIncludeCategoriesInKeywords() {
  return GetFieldTrialParamByFeatureAsBool(
      kOnDeviceClustering, "include_categories_in_keywords", true);
}

bool ShouldExcludeKeywordsFromNoisyVisits() {
  return GetFieldTrialParamByFeatureAsBool(
      kOnDeviceClustering, "exclude_keywords_from_noisy_visits", false);
}

size_t GetClusteringTasksBatchSize() {
  return GetFieldTrialParamByFeatureAsInt(
      features::kSplitClusteringTasksToSmallerBatches,
      "clustering_task_batch_size", 250);
}

bool ShouldSplitClustersAtSearchVisits() {
  return GetFieldTrialParamByFeatureAsBool(
      kOnDeviceClustering, "split_clusters_at_search_visits", true);
}

}  // namespace features
}  // namespace history_clusters
