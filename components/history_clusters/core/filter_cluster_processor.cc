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
  return filter_params.min_visits > 0 ||
         filter_params.min_visits_with_images > 0 ||
         !filter_params.categories_allowlist.empty() ||
         !filter_params.categories_blocklist.empty() ||
         filter_params.is_search_initiated ||
         filter_params.has_related_searches ||
         filter_params.is_shown_on_prominent_ui_surfaces ||
         filter_params.max_clusters > 0;
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

// Returns whether `cluster` could possibly be classified as one of the
// categories in `categories`.
bool IsClusterInCategories(const history::Cluster& cluster,
                           const base::flat_set<std::string>& categories) {
  for (const auto& visit : cluster.visits) {
    if (!IsShownVisitCandidate(visit)) {
      continue;
    }

    if (IsVisitInCategories(visit, categories)) {
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

  if (filter_params_->max_clusters > 0) {
    SortClustersUsingFilterParams(clusters);

    if (clusters->size() > filter_params_->max_clusters) {
      clusters->resize(filter_params_->max_clusters);
    }
  }
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

void FilterClusterProcessor::SortClustersUsingFilterParams(
    std::vector<history::Cluster>* clusters) const {
  // Within each cluster, sort visits.
  for (auto& cluster : *clusters) {
    StableSortVisits(cluster.visits);
  }

  // After that, sort clusters based on `filter_params_`.
  base::ranges::stable_sort(*clusters, [this](const auto& c1, const auto& c2) {
    if (c1.visits.empty()) {
      return false;
    }
    if (c2.visits.empty()) {
      return true;
    }

    // Boost categories if provided.
    if (!filter_params_->categories_boostlist.empty()) {
      bool c1_has_visit_in_categories =
          IsClusterInCategories(c1, filter_params_->categories_boostlist);
      bool c2_has_visit_in_categories =
          IsClusterInCategories(c2, filter_params_->categories_boostlist);

      if (c1_has_visit_in_categories ^ c2_has_visit_in_categories) {
        return c1_has_visit_in_categories;
      }
    }

    // Otherwise, fall back to reverse chronological.
    base::Time c1_time = c1.visits.front().annotated_visit.visit_row.visit_time;
    base::Time c2_time = c2.visits.front().annotated_visit.visit_row.visit_time;

    // Use c1 > c2 to get more recent clusters BEFORE older clusters.
    return c1_time > c2_time;
  });
}

}  // namespace history_clusters
