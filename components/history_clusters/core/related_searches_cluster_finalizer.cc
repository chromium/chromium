// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/related_searches_cluster_finalizer.h"

#include "base/containers/contains.h"
#include "components/history_clusters/core/on_device_clustering_util.h"

namespace history_clusters {

RelatedSearchesClusterFinalizer::RelatedSearchesClusterFinalizer() = default;
RelatedSearchesClusterFinalizer::~RelatedSearchesClusterFinalizer() = default;

void RelatedSearchesClusterFinalizer::FinalizeCluster(
    history::Cluster& cluster) {
  constexpr size_t kMaxRelatedSearches = 5;

  for (const auto& visit : cluster.visits) {
    // Coalesce the unique related searches of this visit into the cluster
    // until the cap is reached.
    for (const auto& search_query :
         visit.annotated_visit.content_annotations.related_searches) {
      if (cluster.related_searches.size() >= kMaxRelatedSearches) {
        return;
      }

      if (!base::Contains(cluster.related_searches, search_query)) {
        cluster.related_searches.push_back(search_query);
      }
    }
  }
}

}  // namespace history_clusters
