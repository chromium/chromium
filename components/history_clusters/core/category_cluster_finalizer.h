// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_CATEGORY_CLUSTER_FINALIZER_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_CATEGORY_CLUSTER_FINALIZER_H_

#include "components/history_clusters/core/cluster_finalizer.h"

namespace history_clusters {

// A ClusterFinalizer that determines whether a cluster represents a complex
// task or not based on the categories associated with its visits.
class CategoryClusterFinalizer : public ClusterFinalizer {
 public:
  explicit CategoryClusterFinalizer();
  ~CategoryClusterFinalizer() override;

  // ClusterFinalizer:
  void FinalizeCluster(history::Cluster& cluster) override;
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_CATEGORY_CLUSTER_FINALIZER_H_
