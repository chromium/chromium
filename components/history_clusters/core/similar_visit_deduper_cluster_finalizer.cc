// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/similar_visit_deduper_cluster_finalizer.h"

#include "base/ranges/algorithm.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/on_device_clustering_util.h"

namespace history_clusters {

namespace {

struct SimilarVisit {
  SimilarVisit() = default;
  explicit SimilarVisit(const history::ClusterVisit& visit)
      : title(visit.annotated_visit.url_row.title()),
        url_for_deduping(visit.url_for_deduping) {}
  SimilarVisit(const SimilarVisit&) = default;
  ~SimilarVisit() = default;

  std::u16string title;
  GURL url_for_deduping;

  struct Comp {
    bool operator()(const SimilarVisit& lhs, const SimilarVisit& rhs) const {
      if (lhs.title != rhs.title)
        return lhs.title < rhs.title;
      return lhs.url_for_deduping < rhs.url_for_deduping;
    }
  };
};

}  // namespace

SimilarVisitDeduperClusterFinalizer::SimilarVisitDeduperClusterFinalizer() =
    default;
SimilarVisitDeduperClusterFinalizer::~SimilarVisitDeduperClusterFinalizer() =
    default;

void SimilarVisitDeduperClusterFinalizer::FinalizeCluster(
    history::Cluster& cluster) {
  base::flat_map<SimilarVisit, history::ClusterVisit*, SimilarVisit::Comp>
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
            DCHECK(it != similar_visit_to_canonical_visits.end());
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
