// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_QUERY_CLUSTERS_STATE_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_QUERY_CLUSTERS_STATE_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/history/core/browser/history_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace history_clusters {

class HistoryClustersService;

// This object encapsulates the results of a query to HistoryClustersService.
// It manages fetching more pages from the clustering backend as the user
// scrolls down.
//
// In the future, it will also manage reusing results for new searches, as well
// as collapsing duplicate clusters across pages.
//
// It's the history_clusters equivalent to history::QueryHistoryState.
class QueryClustersState {
 public:
  explicit QueryClustersState(base::WeakPtr<HistoryClustersService> service);
  ~QueryClustersState();

  QueryClustersState(const QueryClustersState&) = delete;

 private:
  const base::WeakPtr<HistoryClustersService> service_;

  std::vector<history::Cluster> clusters_;

  // A nullopt `continuation_end_time` means we have exhausted History.
  // Note that this differs from History itself, which uses base::Time() as the
  // value to indicate we've exhausted history. I've found that to be not
  // explicit enough in practice. This value will never be base::Time().
  absl::optional<base::Time> continuation_end_time_;
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_QUERY_CLUSTERS_STATE_H_
