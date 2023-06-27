// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/cluster_interaction_state_processor.h"
#include <algorithm>
#include <string>
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/cluster_processor.h"
#include "components/history_clusters/core/history_clusters_types.h"

namespace history_clusters {

ClusterInteractionStateProcessor::ClusterInteractionStateProcessor(
    QueryClustersFilterParams& filter_params)
    : filter_params_(filter_params) {}
ClusterInteractionStateProcessor::~ClusterInteractionStateProcessor() = default;

void ClusterInteractionStateProcessor::ProcessClusters(
    std::vector<history::Cluster>* clusters) {
  DCHECK(clusters);
  if (clusters->empty()) {
    return;
  }

  // Populate search terms marked as done and hidden.
  base::flat_set<std::u16string> hidden_search_terms;
  base::flat_set<std::u16string> done_search_terms;
  for (const auto& cluster : *clusters) {
    for (const auto& cluster_visit : cluster.visits) {
      if (cluster_visit.interaction_state ==
          history::ClusterVisit::InteractionState::kHidden) {
        if (!cluster_visit.annotated_visit.content_annotations.search_terms
                 .empty()) {
          hidden_search_terms.insert(
              cluster_visit.annotated_visit.content_annotations.search_terms);
        }
      }
      if (cluster_visit.interaction_state ==
          history::ClusterVisit::InteractionState::kDone) {
        if (!cluster_visit.annotated_visit.content_annotations.search_terms
                 .empty()) {
          done_search_terms.insert(
              cluster_visit.annotated_visit.content_annotations.search_terms);
        }
      }
    }
  }

  // Remove visits having hidden search terms.
  if (filter_params_->filter_hidden_visits) {
    for (auto& cluster : *clusters) {
      cluster.visits.erase(
          base::ranges::remove_if(cluster.visits.begin(), cluster.visits.end(),
                                  [&](history::ClusterVisit cluster_visit) {
                                    return hidden_search_terms.contains(
                                        cluster_visit.annotated_visit
                                            .content_annotations.search_terms);
                                  }),
          cluster.visits.end());
    }
  }

  // Remove cluster if any of it's visits have done search terms.
  if (filter_params_->filter_done_clusters) {
    clusters->erase(base::ranges::remove_if(
                        clusters->begin(), clusters->end(),
                        [&](history::Cluster cluster) {
                          for (const auto& visit : cluster.visits) {
                            if (done_search_terms.contains(
                                    visit.annotated_visit.content_annotations
                                        .search_terms)) {
                              return true;
                            }
                          }
                          return false;
                        }),
                    clusters->end());
  }

  // Remove clusters with empty visits.
  clusters->erase(base::ranges::remove_if(clusters->begin(), clusters->end(),
                                          [](history::Cluster cluster) {
                                            return cluster.visits.empty();
                                          }),
                  clusters->end());
}

}  // namespace history_clusters
