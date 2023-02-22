// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/filter_cluster_processor.h"

#include <iterator>

#include "base/metrics/histogram_functions.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/history_clusters_types.h"
#include "components/history_clusters/core/history_clusters_util.h"

namespace history_clusters {

namespace {

// Returns whether `filter_params` is a filter that would actually filter
// clusters out.
bool IsFunctionalFilter(QueryClustersFilterParams filter_params) {
  return filter_params.min_visits_with_images > 0 ||
         !filter_params.categories.empty() ||
         filter_params.is_search_initiated ||
         filter_params.has_related_searches;
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

// Returns whether `cluster` is included in filter as specified by
// `filter_params`.
bool IsIncludedInFilter(
    const history::Cluster& cluster,
    const raw_ref<QueryClustersFilterParams> filter_params) {
  int num_visits_with_images = 0;
  size_t num_visits_in_allowed_categories = 0;
  bool is_search_initiated = false;
  bool has_related_searches = false;

  for (const auto& visit : cluster.visits) {
    if (visit.annotated_visit.content_annotations.has_url_keyed_image) {
      num_visits_with_images++;
    }
    if (!filter_params->categories.empty() &&
        IsVisitInCategories(visit, filter_params->categories)) {
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
  }

  // TODO(b/265301309): Figure out if we should just incorporate the
  // sensitivity, single visit, and other filters here instead.

  // TODO(b/265301309): Add filter reason metrics.

  if (num_visits_with_images < filter_params->min_visits_with_images) {
    return false;
  }
  if (!filter_params->categories.empty() &&
      num_visits_in_allowed_categories <
          GetConfig().number_interesting_visits_filter_threshold) {
    return false;
  }
  if (filter_params->is_search_initiated && !is_search_initiated) {
    return false;
  }
  if (filter_params->has_related_searches && !has_related_searches) {
    return false;
  }

  // If it gets here, the cluster has passed all filter conditions.
  return true;
}

}  // namespace

FilterClusterProcessor::FilterClusterProcessor(
    ClusteringRequestSource clustering_request_source,
    QueryClustersFilterParams& filter_params)
    : clustering_request_source_(clustering_request_source),
      should_run_filter_(IsFunctionalFilter(filter_params)),
      filter_params_(filter_params) {}
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

  clusters->erase(base::ranges::remove_if(*clusters,
                                          [&](auto& cluster) {
                                            return !IsIncludedInFilter(
                                                cluster, filter_params_);
                                          }),
                  clusters->end());

  base::UmaHistogramCounts1000(
      "History.Clusters.Backend.FilterClusterProcessor.NumClusters.PostFilter" +
          GetHistogramNameSliceForRequestSource(clustering_request_source_),
      clusters->size());
}

}  // namespace history_clusters
