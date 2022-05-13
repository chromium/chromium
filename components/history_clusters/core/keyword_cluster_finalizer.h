// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_KEYWORD_CLUSTER_FINALIZER_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_KEYWORD_CLUSTER_FINALIZER_H_

#include <string>

#include "base/containers/flat_set.h"
#include "components/history_clusters/core/cluster_finalizer.h"

namespace optimization_guide {
struct EntityMetadata;
}  // namespace optimization_guide

namespace history_clusters {

// A cluster finalizer that determines the set of keywords for a given cluster.
class KeywordClusterFinalizer : public ClusterFinalizer {
 public:
  explicit KeywordClusterFinalizer(
      const base::flat_map<std::string, optimization_guide::EntityMetadata>&
          entity_metadata_map);
  ~KeywordClusterFinalizer() override;

  // ClusterFinalizer:
  void FinalizeCluster(history::Cluster& cluster) override;

 private:
  // A map from human readable entity name to the metadata associated with that
  // entity name.
  const base::flat_map<std::string, optimization_guide::EntityMetadata>
      entity_metadata_map_;
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_KEYWORD_CLUSTER_FINALIZER_H_
