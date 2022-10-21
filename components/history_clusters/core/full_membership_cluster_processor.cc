// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/full_membership_cluster_processor.h"

#include <iterator>

#include "base/containers/contains.h"
#include "components/history_clusters/core/on_device_clustering_util.h"
#include "components/history_clusters/core/similar_visit.h"

namespace history_clusters {

FullMembershipClusterProcessor::FullMembershipClusterProcessor() = default;
FullMembershipClusterProcessor::~FullMembershipClusterProcessor() = default;

void FullMembershipClusterProcessor::ProcessClusters(
    std::vector<history::Cluster>* clusters) {
  std::vector<base::flat_set<SimilarVisit, SimilarVisit::Comp>> visit_sets(
      clusters->size());
  for (size_t i = 0; i < clusters->size(); i++) {
    for (const auto& visit : clusters->at(i).visits) {
      visit_sets[i].insert(SimilarVisit(visit));
    }
  }

  for (size_t i = 0; i < clusters->size(); i++) {
    if (clusters->at(i).visits.empty()) {
      continue;
    }
    // Greedily combine clusters by checking if a cluster has all the same
    // visits as another one.
    for (size_t j = 0; j < clusters->size(); j++) {
      if (i == j || clusters->at(j).visits.empty()) {
        continue;
      }

      // See if everything in the smaller cluster is in the larger cluster.
      size_t cluster_i_size = clusters->at(i).visits.size();
      size_t cluster_j_size = clusters->at(j).visits.size();
      const std::vector<history::ClusterVisit>& smaller_cluster_visits =
          cluster_i_size < cluster_j_size ? clusters->at(i).visits
                                          : clusters->at(j).visits;
      base::flat_set<SimilarVisit, SimilarVisit::Comp>& larger_cluster_visits =
          cluster_i_size < cluster_j_size ? visit_sets[j] : visit_sets[i];

      bool has_full_set_membership = true;
      for (const auto& visit : smaller_cluster_visits) {
        SimilarVisit similar_visit(visit);
        if (!base::Contains(larger_cluster_visits, SimilarVisit(visit))) {
          has_full_set_membership = false;
          break;
        }
      }

      if (has_full_set_membership) {
        // Add the visits to the aggregated cluster.
        visit_sets[i].insert(std::make_move_iterator(visit_sets[j].begin()),
                             std::make_move_iterator(visit_sets[j].end()));
        AppendClusterVisits(clusters->at(i), clusters->at(j));
      }
    }
  }

  RemoveEmptyClusters(clusters);
}

}  // namespace history_clusters
