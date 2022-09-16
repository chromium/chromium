// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/category_cluster_finalizer.h"

#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/cluster_metrics_utils.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/on_device_clustering_features.h"
#include "components/history_clusters/core/on_device_clustering_util.h"

namespace history_clusters {

CategoryClusterFinalizer::CategoryClusterFinalizer() = default;
CategoryClusterFinalizer::~CategoryClusterFinalizer() = default;

void CategoryClusterFinalizer::FinalizeCluster(history::Cluster& cluster) {
  const base::flat_set<std::string>& categories_for_filtering =
      GetConfig().categories_for_filtering;
  size_t interesting_visit_cnt = 0;

  ScopedFilterClusterMetricsRecorder metrics_recorder("Category");
  for (const auto& visit : cluster.visits) {
    for (const auto& category : visit.annotated_visit.content_annotations
                                    .model_annotations.categories) {
      if (categories_for_filtering.find(category.id) !=
          categories_for_filtering.end()) {
        interesting_visit_cnt++;
        break;
      }
    }

    if (interesting_visit_cnt >=
        GetConfig().number_interesting_visits_filter_threshold) {
      return;
    }
  }

  // If we check all the visits in the cluster and all have categories that are
  // not representative of Journeys, then it's probably not interesting so we
  // can hide it.
  cluster.should_show_on_prominent_ui_surfaces = false;
  metrics_recorder.set_was_filtered(true);
}

}  // namespace history_clusters
