// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_CONTENT_ANNOTATIONS_CLUSTER_PROCESSOR_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_CONTENT_ANNOTATIONS_CLUSTER_PROCESSOR_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ref.h"
#include "components/history_clusters/core/cluster_processor.h"

namespace optimization_guide {
struct EntityMetadata;
}  // namespace optimization_guide

namespace history_clusters {

// A cluster processor that combines clusters based on content annotations
// similarity.
class ContentAnnotationsClusterProcessor : public ClusterProcessor {
 public:
  explicit ContentAnnotationsClusterProcessor(
      base::flat_map<std::string, optimization_guide::EntityMetadata>*
          entity_id_to_entity_metadata_map);
  ~ContentAnnotationsClusterProcessor() override;

  // ClusterProcessor:
  void ProcessClusters(std::vector<history::Cluster>* clusters) override;

 private:
  // Creates an occurrence map for `cluster` of the set
  // of entities from each visit.
  base::flat_map<std::string, float> CreateOccurrenceMapForCluster(
      const history::Cluster& cluster);

  // The map from entity ID to entity metadata.
  //
  // Not owned. Guaranteed to outlive `this` and be non-null.
  const raw_ref<base::flat_map<std::string, optimization_guide::EntityMetadata>>
      entity_id_to_entity_metadata_map_;
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_CONTENT_ANNOTATIONS_CLUSTER_PROCESSOR_H_
