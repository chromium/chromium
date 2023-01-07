// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_NOISY_CLUSTER_FINALIZER_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_NOISY_CLUSTER_FINALIZER_H_

#include "components/history_clusters/core/cluster_finalizer.h"

namespace history_clusters {

// A cluster finalizer that finalizes single visit clusters by not showing them.
class NoisyClusterFinalizer : public ClusterFinalizer {
 public:
  NoisyClusterFinalizer();
  ~NoisyClusterFinalizer() override;

  // ClusterFinalizer:
  void FinalizeCluster(history::Cluster& cluster) override;
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_NOISY_CLUSTER_FINALIZER_H_
