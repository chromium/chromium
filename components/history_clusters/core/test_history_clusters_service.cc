// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/test_history_clusters_service.h"

#include "base/task/single_thread_task_runner.h"

namespace history_clusters {

TestHistoryClustersService::TestHistoryClustersService()
    : HistoryClustersService("en-US",
                             /*history_service=*/nullptr,
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
  // If the next call would exceed the size of the specified clusters for each
  // call, set the query to done.
  if (!(call_count_ < clusters_for_call_count_.size())) {
    query_is_done_ = true;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     (call_count_ < clusters_for_call_count_.size())
                         ? clusters_for_call_count_.at(call_count_)
                         : std::vector<history::Cluster>(),
                     query_is_done_
                         ? QueryClustersContinuationParams::DoneParams()
                         : continuation_params));
  call_count_++;

  return nullptr;
}

void TestHistoryClustersService::SetIsJourneysEnabled(
    bool is_journeys_enabled) {
  is_journeys_enabled_ = is_journeys_enabled;
}

void TestHistoryClustersService::SetClustersToReturnOnFirstCall(
    const std::vector<history::Cluster>& clusters) {
  SetClustersToReturnForCalls({{clusters}});
}

void TestHistoryClustersService::SetClustersToReturnForCalls(
    const std::vector<std::vector<history::Cluster>>& clusters_for_call_count) {
  call_count_ = 0;
  clusters_for_call_count_ = clusters_for_call_count;
}

}  // namespace history_clusters
