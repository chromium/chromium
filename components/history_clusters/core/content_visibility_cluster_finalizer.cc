// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/content_visibility_cluster_finalizer.h"

#include "components/history_clusters/core/on_device_clustering_features.h"

namespace history_clusters {

ContentVisibilityClusterFinalizer::ContentVisibilityClusterFinalizer() =
    default;
ContentVisibilityClusterFinalizer::~ContentVisibilityClusterFinalizer() =
    default;

void ContentVisibilityClusterFinalizer::FinalizeCluster(
    history::Cluster& cluster) {
  for (const auto& visit : cluster.visits) {
    float visibility_score = visit.annotated_visit.content_annotations
                                 .model_annotations.visibility_score;
    if (visibility_score < 0) {
      // Scores should be between 0 and 1. If it is below zero, that means this
      // visit wasn't evaluated for visibility.
      continue;
    }
    if (visibility_score < features::ContentVisibilityThreshold()) {
      cluster.should_show_on_prominent_ui_surfaces = false;
      return;
    }
  }
  // If we get here, this is a visible cluster from our point of view. If the
  // previous value was false, this should continue to be false. If the previous
  // value was true, this should continue to be true.
}

}  // namespace history_clusters
