// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_DEBUG_JSONS_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_DEBUG_JSONS_H_

#include <string>
#include <unordered_set>
#include <vector>

#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/history_clusters_service.h"

namespace history_clusters {

// Gets a loggable JSON representation of `visits`.
std::string GetDebugJSONForVisits(
    const std::vector<history::AnnotatedVisit>& visits);

// Gets a loggable JSON representation of `clusters`.
std::string GetDebugJSONForClusters(
    const std::vector<history::Cluster>& clusters);

template <typename T>
std::string GetDebugJSONForKeywordSet(const std::unordered_set<T>& keyword_set);

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_DEBUG_JSONS_H_
