// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/cluster_similarity_heuristics_processor.h"

#include <iterator>
#include <unordered_set>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "components/history_clusters/core/on_device_clustering_util.h"
#include "components/history_clusters/core/similar_visit.h"

namespace history_clusters {

ClusterSimilarityHeuristicsProcessor::ClusterSimilarityHeuristicsProcessor() =
    default;
ClusterSimilarityHeuristicsProcessor::~ClusterSimilarityHeuristicsProcessor() =
    default;

void ClusterSimilarityHeuristicsProcessor::ProcessClusters(
    std::vector<history::Cluster>* clusters) {
  std::vector<std::unordered_set<SimilarVisit, SimilarVisit::Hash,
                                 SimilarVisit::Equals>>
      visit_sets(clusters->size());
  std::vector<std::u16string> cluster_search_terms(clusters->size());
  for (size_t i = 0; i < clusters->size(); i++) {
    // Populate an empty search terms for the cluster.
    cluster_search_terms.push_back(u"");
    for (const auto& visit : clusters->at(i).visits) {
      visit_sets[i].insert(SimilarVisit(visit));
      // Update the search terms of the cluster if empty, based on the visit and
      // check that all search visits in a cluster have the same search terms.
      if (!visit.annotated_visit.content_annotations.search_terms.empty()) {
        if (!cluster_search_terms[i].empty()) {
          base::UmaHistogramBoolean(
              "History.Clusters.Backend.ClusterSimilarityHeuristicsProcessor."
              "ClusterSearchTermOverriden",
              cluster_search_terms[i] !=
                  visit.annotated_visit.content_annotations.search_terms);
        }
        cluster_search_terms[i] =
            visit.annotated_visit.content_annotations.search_terms;
      }
    }
  }

  for (size_t i = 0; i < clusters->size(); i++) {
    if (clusters->at(i).visits.empty()) {
      continue;
    }
    // Greedily combine clusters based on any of the following heuristics:
    //  1. If 2 clusters have the same search terms.
    //  2. If visits of a clusters are subset of another cluster visits.
    for (size_t j = 0; j < clusters->size(); j++) {
      if (i == j || clusters->at(j).visits.empty()) {
        continue;
      }

      // Lambda to check if search terms of the clusters are the same and
      // non-empty.
      auto has_same_search_terms = [&]() {
        return (cluster_search_terms[i] == cluster_search_terms[j]) &&
               !cluster_search_terms[i].empty();
      };

      // Lambda to check if everything in the smaller cluster is in the larger
      // cluster.
      auto has_full_set_membership = [&]() {
        size_t cluster_i_size = clusters->at(i).visits.size();
        size_t cluster_j_size = clusters->at(j).visits.size();
        const std::vector<history::ClusterVisit>& smaller_cluster_visits =
            cluster_i_size < cluster_j_size ? clusters->at(i).visits
                                            : clusters->at(j).visits;
        std::unordered_set<SimilarVisit, SimilarVisit::Hash,
                           SimilarVisit::Equals>& larger_cluster_visits =
            cluster_i_size < cluster_j_size ? visit_sets[j] : visit_sets[i];

        bool has_full_set_membership = true;
        for (const auto& visit : smaller_cluster_visits) {
          if (!base::Contains(larger_cluster_visits, SimilarVisit(visit))) {
            has_full_set_membership = false;
            break;
          }
        }
        return has_full_set_membership;
      };

      if (has_same_search_terms() || has_full_set_membership()) {
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
