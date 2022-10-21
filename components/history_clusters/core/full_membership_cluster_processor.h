// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_FULL_MEMBERSHIP_CLUSTER_PROCESSOR_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_FULL_MEMBERSHIP_CLUSTER_PROCESSOR_H_

#include "components/history_clusters/core/cluster_processor.h"

namespace history_clusters {

// A cluster processor that combines clusters if all visits in one cluster
// contains visits that look the same as visits in another cluster.
class FullMembershipClusterProcessor : public ClusterProcessor {
 public:
  explicit FullMembershipClusterProcessor();
  ~FullMembershipClusterProcessor() override;

  // ClusterProcessor:
  void ProcessClusters(std::vector<history::Cluster>* clusters) override;
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_FULL_MEMBERSHIP_CLUSTER_PROCESSOR_H_
