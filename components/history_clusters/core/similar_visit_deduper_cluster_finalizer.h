// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_SIMILAR_VISIT_DEDUPER_CLUSTER_FINALIZER_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_SIMILAR_VISIT_DEDUPER_CLUSTER_FINALIZER_H_

#include "components/history_clusters/core/cluster_finalizer.h"

namespace history_clusters {

// A cluster finalizer that dedupes similar visits based on URL and what's shown
// in the UI (i.e. host and title).
class SimilarVisitDeduperClusterFinalizer : public ClusterFinalizer {
 public:
  SimilarVisitDeduperClusterFinalizer();
  ~SimilarVisitDeduperClusterFinalizer() override;

  // ClusterFinalizer:
  void FinalizeCluster(history::Cluster& cluster) override;
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_SIMILAR_VISIT_DEDUPER_CLUSTER_FINALIZER_H_
