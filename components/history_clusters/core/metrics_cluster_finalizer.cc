// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/metrics_cluster_finalizer.h"

#include "base/metrics/histogram_functions.h"

namespace history_clusters {

MetricsClusterFinalizer::MetricsClusterFinalizer() = default;
MetricsClusterFinalizer::~MetricsClusterFinalizer() = default;

void MetricsClusterFinalizer::FinalizeCluster(history::Cluster& cluster) {
  base::UmaHistogramCounts100("History.Clusters.Backend.NumVisitsPerCluster",
                              cluster.visits.size());
  base::UmaHistogramCounts100("History.Clusters.Backend.NumKeywordsPerCluster",
                              cluster.keyword_to_data_map.size());

  bool contains_search = false;
  for (const auto& visit : cluster.visits) {
    if (!visit.annotated_visit.content_annotations.search_terms.empty()) {
      contains_search = true;
      break;
    }
  }
  base::UmaHistogramBoolean("History.Clusters.Backend.ClusterContainsSearch",
                            contains_search);
}

}  // namespace history_clusters
