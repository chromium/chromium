// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/url_deduper_cluster_finalizer.h"

#include "base/ranges/algorithm.h"
#include "components/history_clusters/core/on_device_clustering_util.h"

namespace history_clusters {

UrlDeduperClusterFinalizer::UrlDeduperClusterFinalizer() = default;
UrlDeduperClusterFinalizer::~UrlDeduperClusterFinalizer() = default;

void UrlDeduperClusterFinalizer::FinalizeCluster(history::Cluster& cluster) {
  base::flat_map<std::string, history::ClusterVisit*> url_to_canonical_visit;
  // First do a prepass to find the canonical visit for each URL. This simply
  // marks the last visit in `cluster` with any given URL as the canonical one.
  for (auto& visit : cluster.visits) {
    url_to_canonical_visit[visit.url_for_deduping.possibly_invalid_spec()] =
        &visit;
  }

  cluster.visits.erase(
      base::ranges::remove_if(
          cluster.visits,
          [&](auto& visit) {
            // We are guaranteed to find a matching canonical visit, due to our
            // prepass above.
            auto it = url_to_canonical_visit.find(
                visit.url_for_deduping.possibly_invalid_spec());
            DCHECK(it != url_to_canonical_visit.end());
            history::ClusterVisit* canonical_visit = it->second;

            // If a DIFFERENT visit is the canonical visit for this key, merge
            // this visit in, and mark this visit as to be removed.
            if (&visit != canonical_visit) {
              MergeDuplicateVisitIntoCanonicalVisit(std::move(visit),
                                                    *canonical_visit);
              return true;
            }

            return false;
          }),
      cluster.visits.end());
}

}  // namespace history_clusters
