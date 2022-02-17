// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_UTIL_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_UTIL_H_

#include <string>
#include <vector>

#include "components/history/core/browser/history_types.h"
#include "url/gurl.h"

namespace history_clusters {

// Computes a simplified GURL for deduping purposes only. The resulting GURL may
// not be valid or navigable, and is only intended for History Cluster deduping.
//
// Note, this is NOT meant to be applied to Search Result Page URLs. Those
// should be separately canonicalized by TemplateURLService and not sent here.
GURL ComputeURLForDeduping(const GURL& url);

// Filter `clusters` matching `query`. There are additional filters (e.g.
// `max_time`) used when requesting `QueryClusters()`, but this function is only
// responsible for matching `query`.
std::vector<history::Cluster> FilterClustersMatchingQuery(
    std::string query,
    std::vector<history::Cluster> clusters);

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_UTIL_H_
