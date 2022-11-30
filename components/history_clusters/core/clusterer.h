// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_CLUSTERER_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_CLUSTERER_H_

#include <vector>

#include "components/history/core/browser/history_types.h"

namespace history_clusters {

// An object that groups visits into clusters.
class Clusterer {
 public:
  Clusterer();
  ~Clusterer();

  // Groups |visits| into clusters.
  std::vector<history::Cluster> CreateInitialClustersFromVisits(
      std::vector<history::ClusterVisit> visits);
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_CLUSTERER_H_
