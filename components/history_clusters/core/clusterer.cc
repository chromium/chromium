// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/clusterer.h"

#include <unordered_map>

#include "base/containers/adapters.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/on_device_clustering_features.h"
#include "components/history_clusters/core/similar_visit.h"

namespace history_clusters {

namespace {

// Returns whether |visit| should be added to |cluster|.
bool ShouldAddVisitToCluster(const history::ClusterVisit& visit,
                             const history::Cluster& cluster) {
  if ((visit.annotated_visit.visit_row.visit_time -
       cluster.visits.back().annotated_visit.visit_row.visit_time) >
      GetConfig().cluster_navigation_time_cutoff) {
    return false;
  }
  if (!visit.annotated_visit.content_annotations.search_terms.empty()) {
    // If we want to split the clusters at search visits and we are at a search
    // visit, only add the visit to the cluster if the last search visit was
    // also a search visit with the same terms. Also break the cluster if there
    // was not already a search visit already.
    for (const auto& existing_visit : base::Reversed(cluster.visits)) {
      if (!existing_visit.annotated_visit.content_annotations.search_terms
               .empty()) {
        return existing_visit.annotated_visit.content_annotations
                   .search_terms ==
               visit.annotated_visit.content_annotations.search_terms;
      }
    }
    return false;
  }
  return true;
}

}  // namespace

Clusterer::Clusterer() = default;
Clusterer::~Clusterer() = default;

std::vector<history::Cluster> Clusterer::CreateInitialClustersFromVisits(
    std::vector<history::ClusterVisit> visits) {
  // Sort visits by visit time.
  std::sort(visits.begin(), visits.end(),
            [](const history::ClusterVisit& a, const history::ClusterVisit& b) {
              return a.annotated_visit.visit_row < b.annotated_visit.visit_row;
            });

  std::unordered_map<SimilarVisit, size_t, SimilarVisit::Hash,
                     SimilarVisit::Equals>
      similar_visit_to_cluster_map;
  base::flat_map<history::VisitID, size_t> visit_id_to_cluster_map;
  std::vector<history::Cluster> clusters;
  for (auto& visit : visits) {
    std::optional<size_t> cluster_idx;
    std::vector<history::VisitID> previous_visit_ids_to_check;
    if (visit.annotated_visit.opener_visit_of_redirect_chain_start != 0) {
      previous_visit_ids_to_check.push_back(
          visit.annotated_visit.opener_visit_of_redirect_chain_start);
    }
    if (visit.annotated_visit.referring_visit_of_redirect_chain_start != 0) {
      previous_visit_ids_to_check.push_back(
          visit.annotated_visit.referring_visit_of_redirect_chain_start);
    }
    if (!previous_visit_ids_to_check.empty()) {
      // See if we have clustered any of the previous visits with opener taking
      // precedence.
      for (history::VisitID previous_visit_id : previous_visit_ids_to_check) {
        auto it = visit_id_to_cluster_map.find(previous_visit_id);
        if (it != visit_id_to_cluster_map.end()) {
          cluster_idx = it->second;
          break;
        }
      }
    } else {
      // See if we have clustered the URL. (forward-back, reload, etc.)
      auto it = similar_visit_to_cluster_map.find(SimilarVisit(visit));
      if (it != similar_visit_to_cluster_map.end()) {
        cluster_idx = it->second;
      }
    }
    DCHECK(!cluster_idx || (*cluster_idx < clusters.size()));

    // Even if above conditions were met, see if we should add it to the cluster
    // based on the characteristics of the in progress cluster and the current
    // visit we are processing.
    if (cluster_idx) {
      auto& in_progress_cluster = clusters[*cluster_idx];
      if (!ShouldAddVisitToCluster(visit, in_progress_cluster)) {
        // Erase all visits in the cluster from the maps since we no longer
        // want to consider anything in the cluster as a referrer.
        for (const auto& finalized_visit : in_progress_cluster.visits) {
          visit_id_to_cluster_map.erase(
              finalized_visit.annotated_visit.visit_row.visit_id);
          similar_visit_to_cluster_map.erase(SimilarVisit(finalized_visit));
        }

        // Reset the working cluster index so we start a new cluster for this
        // visit.
        cluster_idx = std::nullopt;
      }
    }

    if (!cluster_idx) {
      // Create a new cluster.
      cluster_idx = clusters.size();

      history::Cluster new_cluster;
      clusters.push_back(std::move(new_cluster));
    }

    // Add to mapping.
    visit_id_to_cluster_map[visit.annotated_visit.visit_row.visit_id] =
        *cluster_idx;
    similar_visit_to_cluster_map[SimilarVisit(visit)] = *cluster_idx;

    // By default, the current visit will be assigned a max score of 1.0 until
    // otherwise scored during finalization.
    visit.score = 1.0;
    // Add to its cluster.
    clusters[*cluster_idx].visits.push_back(std::move(visit));
  }

  return clusters;
}

}  // namespace history_clusters
