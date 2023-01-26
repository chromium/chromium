// Copyright 2022 The Chromium Authors
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

/**
 * The following enums must be kept in sync with their respective variants in
 * //tools/metrics/histograms/metadata/history/histograms.xml and
 * //ui/webui/resources/cr_components/history_clusters/history_clusters.mojom
 */

// Actions that can be performed on clusters. The int values are not logged and
// can be changed, but should remain consistent with history_clusters.mojom.
enum class ClusterAction {
  kDeleted = 0,
  kOpenedInTabGroup = 1,
  kRelatedSearchClicked = 2,
  kVisitClicked = 3,
};

// Actions that can be performed on related search items.
enum class RelatedSearchAction {
  kClicked = 0,
};

// Actions that can be performed on visits.
enum class VisitAction {
  kClicked = 0,
  kHidden = 1,
  kDeleted = 2,
};

// Types of visits that can be shown and acted on.
enum class VisitType {
  kSRP = 0,
  kNonSRP = 1,
};

// Returns the string representation of each enum class used for
// logging/histograms.
std::string ClusterActionToString(ClusterAction action);
std::string VisitActionToString(VisitAction action);
std::string VisitTypeToString(VisitType action);
std::string RelatedSearchActionToString(RelatedSearchAction action);

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_CLUSTER_METRICS_UTILS_H_
