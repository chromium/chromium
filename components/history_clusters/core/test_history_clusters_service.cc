// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/test_history_clusters_service.h"

namespace history_clusters {

TestHistoryClustersService::TestHistoryClustersService()
    : HistoryClustersService("en-US",
                             /*history_service=*/nullptr,
                             /*entity_metadata_provider=*/nullptr,
                             /*url_loader_factory=*/nullptr,
                             /*engagement_score_provider=*/nullptr,
                             /*template_url_service=*/nullptr,
                             /*optimization_guide_decider=*/nullptr,
                             /*pref_service=*/nullptr) {}
TestHistoryClustersService::~TestHistoryClustersService() = default;

std::unique_ptr<HistoryClustersServiceTask>
TestHistoryClustersService::QueryClusters(
    ClusteringRequestSource clustering_request_source,
    QueryClustersFilterParams filter_params,
    base::Time begin_time,
    QueryClustersContinuationParams continuation_params,
    bool recluster,
    QueryClustersCallback callback) {
  std::move(callback).Run(clusters_, continuation_params);
  return nullptr;
}

void TestHistoryClustersService::SetClustersToReturn(
    const std::vector<history::Cluster>& clusters) {
  clusters_ = clusters;
}

}  // namespace history_clusters