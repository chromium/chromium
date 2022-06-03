// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/url_deduper_cluster_finalizer.h"

#include "components/history_clusters/core/on_device_clustering_util.h"

namespace history_clusters {

UrlDeduperClusterFinalizer::UrlDeduperClusterFinalizer() = default;
UrlDeduperClusterFinalizer::~UrlDeduperClusterFinalizer() = default;

void UrlDeduperClusterFinalizer::FinalizeCluster(history::Cluster& cluster) {
  base::flat_map<GURL, size_t> url_to_last_visit_idx;
  for (auto visit_it = cluster.visits.rbegin();
       visit_it != cluster.visits.rend(); ++visit_it) {
    auto& visit = *visit_it;
    auto visit_url = visit.normalized_url;
    auto it = url_to_last_visit_idx.find(visit_url);
    if (it != url_to_last_visit_idx.end()) {
      auto last_visit_it = url_to_last_visit_idx.find(visit_url);
      DCHECK(last_visit_it != url_to_last_visit_idx.end());
      DCHECK_LT(last_visit_it->second, cluster.visits.size());
      auto& canonical_visit = cluster.visits.at(last_visit_it->second);
      MergeDuplicateVisitIntoCanonicalVisit(visit, canonical_visit);
    } else {
      url_to_last_visit_idx.insert(
          {visit_url,
           std::distance(cluster.visits.begin(), visit_it.base()) - 1});
    }
  }
}

}  // namespace history_clusters
