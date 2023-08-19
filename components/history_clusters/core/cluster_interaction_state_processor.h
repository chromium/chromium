// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_CLUSTER_INTERACTION_STATE_PROCESSOR_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_CLUSTER_INTERACTION_STATE_PROCESSOR_H_

#include "components/history_clusters/core/cluster_processor.h"
#include "components/history_clusters/core/filter_cluster_processor.h"
#include "components/history_clusters/core/history_clusters_types.h"

namespace history_clusters {

// A cluster processor that filters clusters based on their interaction_state
// flag. This also account for clusters with same search term label.
class ClusterInteractionStateProcessor : public ClusterProcessor {
 public:
  explicit ClusterInteractionStateProcessor(
      QueryClustersFilterParams& filter_params);
  ~ClusterInteractionStateProcessor() override;

  // ClusterProcessor:
  void ProcessClusters(std::vector<history::Cluster>* clusters) override;

 private:
  // Not owned. Guaranteed to outlive `this` and be non-null.
  const raw_ref<QueryClustersFilterParams> filter_params_;
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_CLUSTER_INTERACTION_STATE_PROCESSOR_H_
