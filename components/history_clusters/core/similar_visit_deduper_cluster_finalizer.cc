// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/similar_visit_deduper_cluster_finalizer.h"

#include <unordered_map>

#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/on_device_clustering_util.h"
#include "components/history_clusters/core/similar_visit.h"

namespace history_clusters {

SimilarVisitDeduperClusterFinalizer::SimilarVisitDeduperClusterFinalizer() =
    default;
SimilarVisitDeduperClusterFinalizer::~SimilarVisitDeduperClusterFinalizer() =
    default;

void SimilarVisitDeduperClusterFinalizer::FinalizeCluster(
    history::Cluster& cluster) {
  std::unordered_map<SimilarVisit, history::ClusterVisit*, SimilarVisit::Hash,
                     SimilarVisit::Equals>
      similar_visit_to_canonical_visits;
  // First do a prepass to find the canonical visit for each SimilarVisit key.
  // This simply marks the last visit in `cluster` with any given SimilarVisit
  // key as the canonical one.
  for (auto& visit : cluster.visits) {
    similar_visit_to_canonical_visits[SimilarVisit(visit)] = &visit;
  }

  cluster.visits.erase(
      base::ranges::remove_if(
          cluster.visits,
          [&](auto& visit) {
            // We are guaranteed to find a matching canonical visit, due to our
            // prepass above.
            auto it =
                similar_visit_to_canonical_visits.find(SimilarVisit(visit));
            CHECK(it != similar_visit_to_canonical_visits.end(),
                  base::NotFatalUntil::M130);
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
