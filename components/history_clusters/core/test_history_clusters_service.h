// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_TEST_HISTORY_CLUSTERS_SERVICE_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_TEST_HISTORY_CLUSTERS_SERVICE_H_

#include <vector>

#include "components/history_clusters/core/history_clusters_service.h"

namespace history_clusters {

// An implementation of `HistoryClustersService` that is more usable in tests by
// consumers of history clusters functionality.
class TestHistoryClustersService : public HistoryClustersService {
 public:
  TestHistoryClustersService();
  TestHistoryClustersService(const TestHistoryClustersService&) = delete;
  TestHistoryClustersService& operator=(const TestHistoryClustersService&) =
      delete;
  ~TestHistoryClustersService() override;

  // HistoryClustersService:
  bool IsJourneysEnabledAndVisible() const override;
  std::unique_ptr<HistoryClustersServiceTask> QueryClusters(
      ClusteringRequestSource clustering_request_source,
      QueryClustersFilterParams filter_params,
      base::Time begin_time,
      QueryClustersContinuationParams continuation_params,
      bool recluster,
      QueryClustersCallback callback) override;

  // Sets whether Journeys is enabled.
  void SetIsJourneysEnabled(bool is_journeys_enabled);

  // Sets `clusters` to be the clusters that will return the first time
  // `QueryClusters()` is called. Subsequent calls will return an empty vector.
  // The query will invoke its callback using
  // `QueryClustersContinuationParams::DoneParams()`.
  void SetClustersToReturnOnFirstCall(
      const std::vector<history::Cluster>& clusters);

  // Sets the clusters to return for a particular call count. When a call
  // exceeds the specified clusters data, an empty vector will be returned, and
  // the query will invoke its callback using
  // `QueryClustersContinuationParams::DoneParams()`
  void SetClustersToReturnForCalls(
      const std::vector<std::vector<history::Cluster>>&
          clusters_for_call_count);

 private:
  bool is_journeys_enabled_ = true;
  std::vector<std::vector<history::Cluster>> clusters_for_call_count_;
  // The number of times the `QueryClusters()` function has been called.
  size_t call_count_ = 0;
  bool query_is_done_ = false;
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_TEST_HISTORY_CLUSTERS_SERVICE_H_
