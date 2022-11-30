// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_CLUSTERING_BACKEND_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_CLUSTERING_BACKEND_H_

#include "base/callback.h"
#include "components/history/core/browser/history_types.h"

namespace history_clusters {

enum class ClusteringRequestSource { kKeywordCacheGeneration, kJourneysPage };

// An abstract interface for a swappable clustering backend.
class ClusteringBackend {
 public:
  using ClustersCallback =
      base::OnceCallback<void(std::vector<history::Cluster>)>;

  virtual ~ClusteringBackend() = default;

  // The backend clusters `visits` and returns the results asynchronously via
  // `callback`. See `SortClusters()` in on_device_clustering_util.cc for
  // ordering details.
  virtual void GetClusters(ClusteringRequestSource clustering_request_source,
                           ClustersCallback callback,
                           std::vector<history::AnnotatedVisit> visits) = 0;
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_CLUSTERING_BACKEND_H_
