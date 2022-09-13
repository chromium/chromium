// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/single_domain_cluster_finalizer.h"

#include "components/history_clusters/core/cluster_metrics_utils.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace history_clusters {

namespace {

bool IsSingleDomainCluster(const history::Cluster& cluster) {
  for (size_t i = 1; i < cluster.visits.size(); i++) {
    if (!net::registry_controlled_domains::SameDomainOrHost(
            cluster.visits[0].normalized_url, cluster.visits[i].normalized_url,
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
      return false;
    }
  }
  return true;
}

}  // namespace

SingleDomainClusterFinalizer::SingleDomainClusterFinalizer() = default;
SingleDomainClusterFinalizer::~SingleDomainClusterFinalizer() = default;

void SingleDomainClusterFinalizer::FinalizeCluster(history::Cluster& cluster) {
  ScopedFilterClusterMetricsRecorder metrics_recorder("SingleDomain");
  if (IsSingleDomainCluster(cluster)) {
    cluster.should_show_on_prominent_ui_surfaces = false;
    metrics_recorder.set_was_filtered(true);
  }
}

}  // namespace history_clusters
