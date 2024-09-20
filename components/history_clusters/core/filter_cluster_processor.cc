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
bool IsFunctionalFilter(const QueryClustersFilterParams& filter_params) {
  return filter_params.min_visits > 0 ||
         filter_params.min_visits_with_images > 0 ||
         !filter_params.categories_allowlist.empty() ||
         !filter_params.categories_blocklist.empty() ||
         filter_params.is_search_initiated ||
         filter_params.has_related_searches ||
         filter_params.is_shown_on_prominent_ui_surfaces;
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
    history::Cluster& cluster) const {
  int num_visits_with_images = 0;
  size_t num_visits_in_allowed_categories = 0;
  bool has_visits_in_blocked_categories = false;
  bool is_search_initiated = false;
  bool has_related_searches = false;
  size_t num_interesting_visits = 0;
  int num_visits = 0;
  bool is_content_visible = true;

  for (const auto& visit : cluster.visits) {
    if (!IsShownVisitCandidate(visit)) {
      continue;
    }

    num_visits++;

    if (visit.annotated_visit.content_annotations.has_url_keyed_image &&
        visit.annotated_visit.visit_row.is_known_to_sync) {
      num_visits_with_images++;
    }
    if (!filter_params_->categories_allowlist.empty() &&
        IsVisitInCategories(visit, filter_params_->categories_allowlist)) {
      num_visits_in_allowed_categories++;
    }
    if (!filter_params_->categories_blocklist.empty() &&
        IsVisitInCategories(visit, filter_params_->categories_blocklist)) {
      has_visits_in_blocked_categories = true;
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
  if (num_visits < filter_params_->min_visits) {
    RecordClusterFilterReasonHistogram(clustering_request_source_,
                                       ClusterFilterReason::kNotEnoughVisits);
    matches_filter = false;
  }
  if (num_visits_with_images < filter_params_->min_visits_with_images) {
    RecordClusterFilterReasonHistogram(clustering_request_source_,
                                       ClusterFilterReason::kNotEnoughImages);
    matches_filter = false;
  }
  if (!filter_params_->categories_allowlist.empty() &&
      num_visits_in_allowed_categories <
          GetConfig().number_interesting_visits_filter_threshold) {
    RecordClusterFilterReasonHistogram(clustering_request_source_,
                                       ClusterFilterReason::kNoCategoryMatch);
    matches_filter = false;
  }
  if (!filter_params_->categories_blocklist.empty() &&
      has_visits_in_blocked_categories) {
    RecordClusterFilterReasonHistogram(
        clustering_request_source_, ClusterFilterReason::kHasBlockedCategory);
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
    cluster.should_show_on_prominent_ui_surfaces = true;

    if (engagement_score_provider_is_valid_ &&
        num_interesting_visits <
            GetConfig().number_interesting_visits_filter_threshold) {
      RecordClusterFilterReasonHistogram(
          clustering_request_source_,
          ClusterFilterReason::kNotEnoughInterestingVisits);
      matches_filter = false;
    }
    if (num_visits <= 1) {
      RecordClusterFilterReasonHistogram(clustering_request_source_,
                                         ClusterFilterReason::kSingleVisit);
      matches_filter = false;
    }
    if (!is_content_visible) {
      RecordClusterFilterReasonHistogram(
          clustering_request_source_, ClusterFilterReason::kNotContentVisible);
      matches_filter = false;
    }

    cluster.should_show_on_prominent_ui_surfaces = matches_filter;
  }

  if (matches_filter) {
    RecordClusterFilterReasonHistogram(clustering_request_source_,
                                       ClusterFilterReason::kNotFiltered);
  }

  return matches_filter;
}

}  // namespace history_clusters
