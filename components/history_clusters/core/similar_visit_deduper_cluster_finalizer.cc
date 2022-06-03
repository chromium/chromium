// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/similar_visit_deduper_cluster_finalizer.h"

#include "components/history_clusters/core/on_device_clustering_util.h"

namespace history_clusters {

namespace {

struct SimilarVisit {
  SimilarVisit() = default;
  explicit SimilarVisit(const history::ClusterVisit& visit)
      : title(visit.annotated_visit.url_row.title()),
        host(visit.normalized_url.host()) {}
  SimilarVisit(const SimilarVisit&) = default;
  ~SimilarVisit() = default;

  std::u16string title;
  std::string host;

  struct Comp {
    bool operator()(const SimilarVisit& lhs, const SimilarVisit& rhs) const {
      if (lhs.title != rhs.title)
        return lhs.title < rhs.title;
      return lhs.host < rhs.host;
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
  base::flat_map<SimilarVisit, size_t, SimilarVisit::Comp>
      similar_visit_to_last_visit_idx;
  for (auto visit_it = cluster.visits.rbegin();
       visit_it != cluster.visits.rend(); ++visit_it) {
    auto& visit = *visit_it;
    SimilarVisit similar_visit(visit);
    auto it = similar_visit_to_last_visit_idx.find(similar_visit);
    if (it != similar_visit_to_last_visit_idx.end()) {
      DCHECK(it != similar_visit_to_last_visit_idx.end());
      DCHECK_LT(it->second, cluster.visits.size());
      auto& canonical_visit = cluster.visits.at(it->second);
      MergeDuplicateVisitIntoCanonicalVisit(visit, canonical_visit);
    } else {
      similar_visit_to_last_visit_idx.insert(
          {similar_visit,
           std::distance(cluster.visits.begin(), visit_it.base()) - 1});
    }
  }
}

}  // namespace history_clusters
