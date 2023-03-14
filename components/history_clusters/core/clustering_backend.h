// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_CLUSTERING_BACKEND_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_CLUSTERING_BACKEND_H_

#include "base/functional/callback.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/history_clusters_types.h"

namespace history_clusters {

// An abstract interface for a swappable clustering backend.
class ClusteringBackend {
 public:
  using ClustersCallback =
      base::OnceCallback<void(std::vector<history::Cluster>)>;

  virtual ~ClusteringBackend() = default;

  // The backend clusters `visits` and returns the results asynchronously via
  // `callback`. If `requires_ui_and_triggerability` is true, this runs
  // additional processing steps to calculate metadata about the clusters
  // required for the UI and triggering.
  //
  // TODO(b/259466296): This method will get removed after persisting context
  // clusters at navigation is rolled out. The below two methods will be the
  // only remaining methods in this interface.
  virtual void GetClusters(ClusteringRequestSource clustering_request_source,
                           ClustersCallback callback,
                           std::vector<history::AnnotatedVisit> visits,
                           bool requires_ui_and_triggerability) = 0;

  // Gets the displayable variant of `clusters` that will be shown on the UI
  // surface associated with `clustering_request_source`. This will merge
  // similar clusters, rank visits within the cluster, as well as provide a
  // label. All returned cluster(s) will match `filter_params`. Will return
  // results asynchronously via `callback`.
  virtual void GetClustersForUI(
      ClusteringRequestSource clustering_request_source,
      QueryClustersFilterParams filter_params,
      ClustersCallback callback,
      std::vector<history::Cluster> clusters) = 0;

  // Gets the metadata required for cluster triggerability (e.g. keywords,
  // whether to show on prominent UI surfaces) for each cluster in `clusters`.
  // Will return results asynchronously via `callback`.
  virtual void GetClusterTriggerability(
      ClustersCallback callback,
      std::vector<history::Cluster> clusters) = 0;
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_CLUSTERING_BACKEND_H_
