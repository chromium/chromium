// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_DEBUG_JSONS_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_DEBUG_JSONS_H_

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "components/history/core/browser/history_types.h"

namespace base {
class Time;
}  // namespace base

namespace history_clusters {

// Gets a loggable string representation of `time`.
std::string GetDebugTime(const base::Time time);

// Gets a loggable JSON representation of `visits`.
std::string GetDebugJSONForVisits(
    const std::vector<history::AnnotatedVisit>& visits);

// Gets a loggable JSON representation of `clusters`.
std::string GetDebugJSONForClusters(
    const std::vector<history::Cluster>& clusters);

template <typename T>
std::string GetDebugJSONForUrlKeywordSet(
    const std::unordered_set<T>& keyword_set);

std::string GetDebugJSONForKeywordMap(
    const std::unordered_map<std::u16string, history::ClusterKeywordData>&
        keyword_to_data_map);

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_DEBUG_JSONS_H_
