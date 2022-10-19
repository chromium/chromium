// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_CLUSTER_PROCESSOR_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_CLUSTER_PROCESSOR_H_

#include <vector>

#include "components/history/core/browser/history_types.h"

namespace history_clusters {

// An abstract interface for cluster processors that perform operations on
// clusters.
class ClusterProcessor {
 public:
  virtual ~ClusterProcessor() = default;

  // Performs operations on clusters (i.e. combine, split).
  virtual void ProcessClusters(std::vector<history::Cluster>* clusters) = 0;

 protected:
  ClusterProcessor() = default;
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_CLUSTER_PROCESSOR_H_
