// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/noisy_cluster_finalizer.h"

#include "components/history_clusters/core/cluster_metrics_utils.h"
#include "components/history_clusters/core/on_device_clustering_features.h"
#include "components/history_clusters/core/on_device_clustering_util.h"

namespace history_clusters {

NoisyClusterFinalizer::NoisyClusterFinalizer() = default;
NoisyClusterFinalizer::~NoisyClusterFinalizer() = default;

void NoisyClusterFinalizer::FinalizeCluster(history::Cluster& cluster) {
  size_t interesting_visit_cnt = 0;
  ScopedFilterClusterMetricsRecorder metrics_recorder("NoisyCluster");
  for (const auto& visit : cluster.visits) {
    if (!IsNoisyVisit(visit)) {
      interesting_visit_cnt += 1;
    }
    if (interesting_visit_cnt >=
        features::NumberInterestingVisitsFilterThreshold()) {
      return;
    }
  }

  // If we check all the visits in the cluster and all have high engagement
  // scores, then its probably not interesting so we can hide it.
  cluster.should_show_on_prominent_ui_surfaces = false;
  metrics_recorder.set_was_filtered(true);
}

}  // namespace history_clusters
