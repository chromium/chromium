// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/filter_cluster_processor.h"

#include <iterator>

#include "base/metrics/histogram_functions.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/history_clusters_types.h"
#include "components/history_clusters/core/history_clusters_util.h"
#include "components/history_clusters/core/on_device_clustering_util.h"

namespace history_clusters {

namespace {

void RecordClusterFilterReasonHistogram(
    ClusteringRequestSource clustering_request_source,
    ClusterFilterReason reason) {
  base::UmaHistogramEnumeration(
      "History.Clusters.Backend.FilterClusterProcessor.ClusterFilterReason" +
          GetHistogramNameSliceForRequestSource(clustering_request_source),
      reason);
}

// Returns whether `filter_params` is a filter that would actually filter
// clusters out.
bool IsFunctionalFilter(QueryClustersFilterParams filter_params) {
  return filter_params.min_visits_with_images > 0 ||
         !filter_params.categories.empty() ||
         filter_params.is_search_initiated ||
         filter_params.has_related_searches ||
         filter_params.is_shown_on_prominent_ui_surfaces;
}

// Returns whether `visit` could possibly be classified as one of the categories
// in `categories`.
bool IsVisitInCategories(const history::ClusterVisit& visit,
                         const base::flat_set<std::string>& categories) {
  for (const auto& visit_category :
       visit.annotated_visit.content_annotations.model_annotations.categories) {
    if (categories.contains(visit_category.id)) {
      return true;
    }
  }
  return false;
}

}  // namespace

FilterClusterProcessor::FilterClusterProcessor(
    ClusteringRequestSource clustering_request_source,
    QueryClustersFilterParams& filter_params,
    bool engagement_score_provider_is_valid)
    : clustering_request_source_(clustering_request_source),
      should_run_filter_(IsFunctionalFilter(filter_params)),
      filter_params_(filter_params),
      engagement_score_provider_is_valid_(engagement_score_provider_is_valid) {}
FilterClusterProcessor::~FilterClusterProcessor() = default;

void FilterClusterProcessor::ProcessClusters(
    std::vector<history::Cluster>* clusters) {
  if (!should_run_filter_) {
    return;
  }

  base::UmaHistogramCounts1000(
      "History.Clusters.Backend.FilterClusterProcessor.NumClusters.PreFilter" +
          GetHistogramNameSliceForRequestSource(clustering_request_source_),
      clusters->size());

  clusters->erase(
      base::ranges::remove_if(
          *clusters,
          [&](auto& cluster) { return !DoesClusterMatchFilter(cluster); }),
      clusters->end());

  base::UmaHistogramCounts1000(
      "History.Clusters.Backend.FilterClusterProcessor.NumClusters.PostFilter" +
          GetHistogramNameSliceForRequestSource(clustering_request_source_),
      clusters->size());
}

bool FilterClusterProcessor::DoesClusterMatchFilter(
    const history::Cluster& cluster) const {
  int num_visits_with_images = 0;
  size_t num_visits_in_allowed_categories = 0;
  bool is_search_initiated = false;
  bool has_related_searches = false;
  size_t num_interesting_visits = 0;
  bool is_content_visible = true;

  for (const auto& visit : cluster.visits) {
    if (visit.annotated_visit.content_annotations.has_url_keyed_image) {
      num_visits_with_images++;
    }
    if (!filter_params_->categories.empty() &&
        IsVisitInCategories(visit, filter_params_->categories)) {
      num_visits_in_allowed_categories++;
    }
    if (!is_search_initiated &&
        !visit.annotated_visit.content_annotations.search_terms.empty()) {
      is_search_initiated = true;
    }
    if (!has_related_searches &&
        !visit.annotated_visit.content_annotations.related_searches.empty()) {
      has_related_searches = true;
    }
    if (engagement_score_provider_is_valid_ && !IsNoisyVisit(visit)) {
      num_interesting_visits++;
    }
    if (is_content_visible) {
      float visibility_score = visit.annotated_visit.content_annotations
                                   .model_annotations.visibility_score;
      if (visibility_score >= 0 &&
          visibility_score < GetConfig().content_visibility_threshold) {
        is_content_visible = false;
      }
    }
  }

  bool matches_filter = true;
  if (num_visits_with_images < filter_params_->min_visits_with_images) {
    RecordClusterFilterReasonHistogram(clustering_request_source_,
                                       ClusterFilterReason::kNotEnoughImages);
    matches_filter = false;
  }
  if (!filter_params_->categories.empty() &&
      num_visits_in_allowed_categories <
          GetConfig().number_interesting_visits_filter_threshold) {
    RecordClusterFilterReasonHistogram(clustering_request_source_,
                                       ClusterFilterReason::kNoCategoryMatch);
    matches_filter = false;
  }
  if (filter_params_->is_search_initiated && !is_search_initiated) {
    RecordClusterFilterReasonHistogram(
        clustering_request_source_, ClusterFilterReason::kNotSearchInitiated);
    matches_filter = false;
  }
  if (filter_params_->has_related_searches && !has_related_searches) {
    RecordClusterFilterReasonHistogram(clustering_request_source_,
                                       ClusterFilterReason::kNoRelatedSearches);
    matches_filter = false;
  }
  if (filter_params_->is_shown_on_prominent_ui_surfaces) {
    if (engagement_score_provider_is_valid_ &&
        num_interesting_visits <
            GetConfig().number_interesting_visits_filter_threshold) {
      RecordClusterFilterReasonHistogram(
          clustering_request_source_,
          ClusterFilterReason::kNotEnoughInterestingVisits);
      matches_filter = false;
    }
    if (cluster.visits.size() <= 1) {
      RecordClusterFilterReasonHistogram(clustering_request_source_,
                                         ClusterFilterReason::kSingleVisit);
      matches_filter = false;
    }
    if (!is_content_visible) {
      RecordClusterFilterReasonHistogram(
          clustering_request_source_, ClusterFilterReason::kNotContentVisible);
      matches_filter = false;
    }
  }

  if (matches_filter) {
    RecordClusterFilterReasonHistogram(clustering_request_source_,
                                       ClusterFilterReason::kNotFiltered);
  }

  return matches_filter;
}

}  // namespace history_clusters
