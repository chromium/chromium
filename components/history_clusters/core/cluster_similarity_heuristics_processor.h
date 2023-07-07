// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_CLUSTER_SIMILARITY_HEURISTICS_PROCESSOR_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_CLUSTER_SIMILARITY_HEURISTICS_PROCESSOR_H_

#include "components/history_clusters/core/cluster_processor.h"

namespace history_clusters {

// A cluster processor that combines 2 clusters based on any of the following
// heuristics:
//  1. If 2 clusters have the same search terms.
//  2. If visits of a clusters are subset of another cluster visits.
class ClusterSimilarityHeuristicsProcessor : public ClusterProcessor {
 public:
  explicit ClusterSimilarityHeuristicsProcessor();
  ~ClusterSimilarityHeuristicsProcessor() override;

  // ClusterProcessor:
  void ProcessClusters(std::vector<history::Cluster>* clusters) override;
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_CLUSTER_SIMILARITY_HEURISTICS_PROCESSOR_H_
