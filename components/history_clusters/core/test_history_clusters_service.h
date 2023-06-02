// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_TEST_HISTORY_CLUSTERS_SERVICE_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_TEST_HISTORY_CLUSTERS_SERVICE_H_

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

  // Sets `clusters` to be the clusters that always get returned when
  // `QueryClusters()` is called. If `exhausted_all_visits` is true, the next
  // query will invoke its callback using
  // `QueryClustersContinuationParams::DoneParams()`.
  void SetClustersToReturn(const std::vector<history::Cluster>& clusters,
                           bool exhausted_all_visits = true);

 private:
  bool is_journeys_enabled_ = true;
  std::vector<history::Cluster> clusters_;
  bool next_query_is_done_ = false;
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_TEST_HISTORY_CLUSTERS_SERVICE_H_
