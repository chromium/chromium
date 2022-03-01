// Copyright 2021 The Chromium Authors. All rights reserved.
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
  // `callback`. `visits` can be passed in arbitrary order, and the resulting
  // clusters can be in arbitrary order too. Caller is responsible for sorting
  // the output however they want it.
  virtual void GetClusters(ClusteringRequestSource clustering_request_source,
                           ClustersCallback callback,
                           std::vector<history::AnnotatedVisit> visits) = 0;
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_CLUSTERING_BACKEND_H_
