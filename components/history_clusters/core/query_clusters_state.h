// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_QUERY_CLUSTERS_STATE_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_QUERY_CLUSTERS_STATE_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/history_clusters_types.h"
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
  // `is_continuation` is true for all 'next-page' responses, but false for the
  // first page.
  using ResultCallback =
      base::OnceCallback<void(const std::string& query,
                              std::vector<history::Cluster> cluster_batch,
                              bool can_load_more,
                              bool is_continuation)>;

  QueryClustersState(base::WeakPtr<HistoryClustersService> service,
                     const std::string& query);
  ~QueryClustersState();

  QueryClustersState(const QueryClustersState&) = delete;

  // Returns the current query the state contains.
  const std::string& query() const { return query_; }

  // Used to request another batch of clusters of the same query.
  void LoadNextBatchOfClusters(ResultCallback callback);

 private:
  friend class QueryClustersStateTest;

  // Private class containing state that's only accessed on
  // `post_processing_task_runner`.
  class PostProcessor;

  // Callback to `LoadNextBatchOfClusters()`.
  void OnGotRawClusters(base::TimeTicks query_start_time,
                        ResultCallback callback,
                        std::vector<history::Cluster> clusters,
                        base::Time continuation_end_time) const;

  // Callback to `OnGotRawClusters()`.
  void OnGotClusters(base::ElapsedTimer post_processing_timer,
                     size_t clusters_from_backend_count,
                     base::TimeTicks query_start_time,
                     ResultCallback callback,
                     base::Time continuation_end_time,
                     std::vector<history::Cluster> clusters);

  // A weak pointer to the service in case we outlive the service.
  // Never nullptr, except in unit tests.
  const base::WeakPtr<HistoryClustersService> service_;

  // The string query the user entered into the searchbox.
  const std::string query_;

  // A nullopt `continuation_end_time` means we have exhausted History.
  // Note that this differs from History itself, which uses base::Time() as the
  // value to indicate we've exhausted history. I've found that to be not
  // explicit enough in practice. This value will never be base::Time().
  absl::optional<base::Time> continuation_end_time_;

  // True for all 'next-page' responses, but false for the first page.
  bool is_continuation_ = false;

  // Used only to fast-cancel tasks in case we are destroyed.
  base::CancelableTaskTracker task_tracker_;

  // A task runner to run all the post-processing tasks on.
  scoped_refptr<base::SequencedTaskRunner> post_processing_task_runner_;

  // The private state used for post-processing. It's created on the main
  // thread, but used and freed on `post_processing_task_runner`.
  scoped_refptr<PostProcessor> post_processing_state_;

  base::WeakPtrFactory<QueryClustersState> weak_factory_{this};
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_QUERY_CLUSTERS_STATE_H_
