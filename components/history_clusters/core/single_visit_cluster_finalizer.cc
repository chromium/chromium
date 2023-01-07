// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/single_visit_cluster_finalizer.h"

#include "components/history_clusters/core/cluster_metrics_utils.h"
#include "components/history_clusters/core/on_device_clustering_util.h"

namespace history_clusters {

SingleVisitClusterFinalizer::SingleVisitClusterFinalizer() = default;
SingleVisitClusterFinalizer::~SingleVisitClusterFinalizer() = default;

void SingleVisitClusterFinalizer::FinalizeCluster(history::Cluster& cluster) {
  ScopedFilterClusterMetricsRecorder metrics_recorder("SingleVisit");
  if (cluster.visits.size() <= 1) {
    cluster.should_show_on_prominent_ui_surfaces = false;
    metrics_recorder.set_was_filtered(true);
  }
}

}  // namespace history_clusters
