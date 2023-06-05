// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/test_history_clusters_service.h"

#include "base/task/single_thread_task_runner.h"

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

bool TestHistoryClustersService::IsJourneysEnabledAndVisible() const {
  return is_journeys_enabled_;
}

std::unique_ptr<HistoryClustersServiceTask>
TestHistoryClustersService::QueryClusters(
    ClusteringRequestSource clustering_request_source,
    QueryClustersFilterParams filter_params,
    base::Time begin_time,
    QueryClustersContinuationParams continuation_params,
    bool recluster,
    QueryClustersCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), clusters_,
                     next_query_is_done_
                         ? QueryClustersContinuationParams::DoneParams()
                         : continuation_params));
  // Set the next query to done so the query eventually finishes.
  next_query_is_done_ = true;
  return nullptr;
}

void TestHistoryClustersService::SetIsJourneysEnabled(
    bool is_journeys_enabled) {
  is_journeys_enabled_ = is_journeys_enabled;
}

void TestHistoryClustersService::SetClustersToReturn(
    const std::vector<history::Cluster>& clusters,
    bool exhausted_all_visits) {
  clusters_ = clusters;
  next_query_is_done_ = exhausted_all_visits;
}

}  // namespace history_clusters