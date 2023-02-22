// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_FILTER_CLUSTER_PROCESSOR_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_FILTER_CLUSTER_PROCESSOR_H_

#include "base/memory/raw_ref.h"
#include "components/history_clusters/core/cluster_processor.h"

namespace history_clusters {

enum class ClusteringRequestSource;
struct QueryClustersFilterParams;

// A cluster processor that removes clusters that do not match the filter.
class FilterClusterProcessor : public ClusterProcessor {
 public:
  explicit FilterClusterProcessor(
      ClusteringRequestSource clustering_request_source,
      QueryClustersFilterParams& filter_params);
  ~FilterClusterProcessor() override;

  // ClusterProcessor:
  void ProcessClusters(std::vector<history::Cluster>* clusters) override;

 private:
  // The clustering request source that requires this filtering. Used for
  // metrics purposes.
  ClusteringRequestSource clustering_request_source_;

  // Whether the logic should be run to see if clusters should be filtered out
  // based on `filter_params_`.
  bool should_run_filter_;

  // The parameters that the clusters are filtered with.
  const raw_ref<QueryClustersFilterParams> filter_params_;
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_FILTER_CLUSTER_PROCESSOR_H_
