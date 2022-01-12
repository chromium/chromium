// Copyright 2021 The Chromium Authors. All rights reserved.
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
    return;
  }

  int canonical_visits_seen = 0;
  base::flat_set<history::VisitID> duplicate_visit_ids =
      CalculateAllDuplicateVisitsForCluster(cluster);
  for (const auto& visit : cluster.visits) {
    if (!duplicate_visit_ids.contains(
            visit.annotated_visit.visit_row.visit_id)) {
      canonical_visits_seen++;
    }
    if (canonical_visits_seen > 1) {
      // Should not explicitly mark as false if multiple canonical visits in the
      // cluster. Just return early.
      return;
    }
  }

  // If we get here, then we have only seen at most 1 canonical visit. Do not
  // show this cluster on prominent UI surfaces.
  cluster.should_show_on_prominent_ui_surfaces = false;
  metrics_recorder.set_was_filtered(true);
}

}  // namespace history_clusters
