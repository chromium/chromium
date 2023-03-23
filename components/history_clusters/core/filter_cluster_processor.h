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

// The reasons why a cluster can be filtered via this processor.
enum class ClusterFilterReason {
  kNotFiltered = 0,
  kNotEnoughImages = 1,
  kNoCategoryMatch = 2,
  kNotSearchInitiated = 3,
  kNoRelatedSearches = 4,
  kNotEnoughInterestingVisits = 5,
  kSingleVisit = 6,
  kNotContentVisible = 7,
  kHasBlockedCategory = 8,
  kNotEnoughVisits = 9,

  // Add above here and make sure to keep `ClusterFilterReason` up to date in
  // enums.xml.

  kMaxValue = kNotEnoughVisits
};

// A cluster processor that removes clusters that do not match the filter.
class FilterClusterProcessor : public ClusterProcessor {
 public:
  explicit FilterClusterProcessor(
      ClusteringRequestSource clustering_request_source,
      QueryClustersFilterParams& filter_params,
      bool engagement_score_provider_is_valid);
  ~FilterClusterProcessor() override;

  // ClusterProcessor:
  void ProcessClusters(std::vector<history::Cluster>* clusters) override;

 private:
  // Returns whether `cluster` matches the filter as specified by
  // `filter_params_`.
  bool DoesClusterMatchFilter(history::Cluster& cluster) const;

  // Sorts clusters based on `filter_params_`.
  void SortClustersUsingFilterParams(
      std::vector<history::Cluster>* clusters) const;

  // The clustering request source that requires this filtering. Used for
  // metrics purposes.
  ClusteringRequestSource clustering_request_source_;

  // Whether the logic should be run to see if clusters should be filtered out
  // based on `filter_params_`.
  bool should_run_filter_;

  // The parameters that the clusters are filtered with.
  const raw_ref<QueryClustersFilterParams> filter_params_;

  // Whether the engagement score provider is valid and the "noisiness" of a
  // visit should be considered as a filter for showing on prominent UI
  // surfaces.
  bool engagement_score_provider_is_valid_;
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_FILTER_CLUSTER_PROCESSOR_H_
