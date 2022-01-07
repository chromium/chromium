// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/clusterer.h"

#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/on_device_clustering_features.h"

namespace history_clusters {

Clusterer::Clusterer() = default;
Clusterer::~Clusterer() = default;

std::vector<history::Cluster> Clusterer::CreateInitialClustersFromVisits(
    const std::vector<history::ClusterVisit>& visits) {
  // Sort visits by visit ID.
  std::vector<history::ClusterVisit> sorted_visits(visits.size());
  std::partial_sort_copy(
      visits.begin(), visits.end(), sorted_visits.begin(), sorted_visits.end(),
      [](const history::ClusterVisit& a, const history::ClusterVisit& b) {
        return a.annotated_visit.visit_row.visit_id <
               b.annotated_visit.visit_row.visit_id;
      });

  base::flat_map<GURL, size_t> url_to_cluster_map;
  base::flat_map<history::VisitID, size_t> visit_id_to_cluster_map;
  std::vector<history::Cluster> clusters;
  for (const auto& visit : sorted_visits) {
    auto visit_url = visit.normalized_url;
    absl::optional<size_t> cluster_idx;
    history::VisitID previous_visit_id =
        (visit.annotated_visit.referring_visit_of_redirect_chain_start != 0)
            ? visit.annotated_visit.referring_visit_of_redirect_chain_start
            : visit.annotated_visit.opener_visit_of_redirect_chain_start;
    if (previous_visit_id != 0) {
      // See if we have clustered the referring visit.
      auto it = visit_id_to_cluster_map.find(previous_visit_id);
      if (it != visit_id_to_cluster_map.end()) {
        cluster_idx = it->second;
      }
    } else {
      // See if we have clustered the URL. (forward-back, reload, etc.)
      auto it = url_to_cluster_map.find(visit_url);
      if (it != url_to_cluster_map.end()) {
        cluster_idx = it->second;
      }
    }
    DCHECK(!cluster_idx || (*cluster_idx < clusters.size()));

    // Even if above conditions were met, add it to a new cluster if the last
    // visit in the cluster's navigation time exceeds a certain duration.
    if (cluster_idx) {
      auto in_progress_cluster = clusters[*cluster_idx];
      auto last_visit_nav_time = in_progress_cluster.visits.back()
                                     .annotated_visit.visit_row.visit_time;
      if ((visit.annotated_visit.visit_row.visit_time - last_visit_nav_time) >
          features::ClusterNavigationTimeCutoff()) {
        // Erase all visits in the cluster from the maps since we no longer
        // want to consider anything in the cluster as a referrer.
        auto finalized_cluster = clusters[*cluster_idx];
        for (const auto& visit : finalized_cluster.visits) {
          visit_id_to_cluster_map.erase(
              visit.annotated_visit.visit_row.visit_id);
          url_to_cluster_map.erase(visit_url);
        }

        // Reset the working cluster index so we start a new cluster for this
        // visit.
        cluster_idx = absl::nullopt;
      }
    }

    // By default, the current visit will be assigned a max score of 1.0 until
    // otherwise scored during finalization.
    history::ClusterVisit default_scored_visit = visit;
    default_scored_visit.score = 1.0;
    if (cluster_idx) {
      clusters[*cluster_idx].visits.push_back(default_scored_visit);
    } else {
      // Add to new cluster.
      cluster_idx = clusters.size();

      history::Cluster new_cluster;
      new_cluster.visits = {default_scored_visit};
      clusters.push_back(std::move(new_cluster));
    }
    visit_id_to_cluster_map[visit.annotated_visit.visit_row.visit_id] =
        *cluster_idx;
    url_to_cluster_map[visit_url] = *cluster_idx;
  }

  return clusters;
}

}  // namespace history_clusters
