// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_CONTENT_ANNOTATIONS_CLUSTER_PROCESSOR_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_CONTENT_ANNOTATIONS_CLUSTER_PROCESSOR_H_

#include "components/history_clusters/core/cluster_processor.h"

namespace history_clusters {

// A cluster processor that combines clusters based on content annotations
// similarity.
class ContentAnnotationsClusterProcessor : public ClusterProcessor {
 public:
  ContentAnnotationsClusterProcessor();
  ~ContentAnnotationsClusterProcessor() override;

  // ClusterProcessor:
  std::vector<history::Cluster> ProcessClusters(
      const std::vector<history::Cluster>& clusters) override;
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_CONTENT_ANNOTATIONS_CLUSTER_PROCESSOR_H_
