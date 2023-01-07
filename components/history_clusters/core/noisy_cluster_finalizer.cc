// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/noisy_cluster_finalizer.h"

#include "components/history_clusters/core/cluster_metrics_utils.h"
#include "components/history_clusters/core/config.h"
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
      // Use the canonical visit's noisiness for all its duplicates too.
      interesting_visit_cnt += 1 + visit.duplicate_visits.size();
    }
    if (interesting_visit_cnt >=
        GetConfig().number_interesting_visits_filter_threshold) {
      return;
    }
  }

  // If we check all the visits in the cluster and all have high engagement
  // scores, then it's probably not interesting so we can hide it.
  cluster.should_show_on_prominent_ui_surfaces = false;
  metrics_recorder.set_was_filtered(true);
}

}  // namespace history_clusters
