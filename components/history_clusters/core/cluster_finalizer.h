// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_CLUSTER_FINALIZER_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_CLUSTER_FINALIZER_H_

#include "base/containers/flat_map.h"
#include "components/history/core/browser/history_types.h"

namespace history_clusters {

class ClusterFinalizer {
 public:
  virtual ~ClusterFinalizer() = default;

  // Performs operations on the final |cluster|, such as deduping and scoring.
  virtual void FinalizeCluster(history::Cluster& cluster) = 0;

 protected:
  ClusterFinalizer() = default;
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_CLUSTER_FINALIZER_H_
