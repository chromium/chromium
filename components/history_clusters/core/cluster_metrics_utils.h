// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_CLUSTER_METRICS_UTILS_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_CLUSTER_METRICS_UTILS_H_

#include "base/metrics/histogram_functions.h"

namespace history_clusters {

// A helper object for recording metrics about whether a cluster was filtered
// for a specified reason. The metric is emitted when the object falls out of
// scope.
class ScopedFilterClusterMetricsRecorder {
 public:
  explicit ScopedFilterClusterMetricsRecorder(
      const std::string& filtered_reason)
      : filtered_reason_(filtered_reason) {}
  ~ScopedFilterClusterMetricsRecorder() {
    base::UmaHistogramBoolean(
        "History.Clusters.Backend.WasClusterFiltered." + filtered_reason_,
        was_filtered_);
  }

  void set_was_filtered(bool was_filtered) { was_filtered_ = was_filtered; }

 private:
  // Whether the cluster associated with this metrics recordered was filtered or
  // not.
  bool was_filtered_ = false;
  // The reason for why the cluster was filtered. Most be one of the items
  // specified in the patterned histogram in
  // tools/metrics/histograms/metadata/history/histograms.xml.
  const std::string filtered_reason_;
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_CLUSTER_METRICS_UTILS_H_
