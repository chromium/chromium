// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_DB_TASKS_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_DB_TASKS_H_

#include <vector>

#include "base/callback.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/history_clusters_service.h"

namespace history_clusters {

// Gets persisted `AnnotatedVisit`s to cluster including both persisted visits
// from the history DB and incomplete visits.
// - We don't want incomplete visits to be mysteriously missing from the
//   Clusters UI. They haven't recorded the page end metrics yet, but that's
//   fine.
// - The history backend will return persisted visits with already computed
//  `referring_visit_of_redirect_chain_start`, while incomplete visits will have
//   to invoke `GetRedirectChainStart()`.
class GetAnnotatedVisitsToCluster : public history::HistoryDBTask {
 public:
  using Callback = base::OnceCallback<void(std::vector<history::AnnotatedVisit>,
                                           base::Time)>;

  GetAnnotatedVisitsToCluster(
      HistoryClustersService::IncompleteVisitMap incomplete_visit_map,
      base::Time end_time,
      size_t max_count,
      Callback callback);
  ~GetAnnotatedVisitsToCluster() override;

  // history::HistoryDBTask:
  bool RunOnDBThread(history::HistoryBackend* backend,
                     history::HistoryDatabase* db) override;
  void DoneRunOnMainThread() override;

 private:
  // Incomplete visits that have history rows and are withing the time frame of
  // the completed visits fetched will be appended to the annotated visits
  // returned for clustering. It's used in the DB thread as each filtered visit
  // will need to fetch its `referring_visit_of_redirect_chain_start`.
  HistoryClustersService::IncompleteVisitMap incomplete_visit_map_;
  // The end time used in the initial history request for completed visits. Used
  // in the DB thread to filter `incomplete_visit_map_`.
  base::Time original_end_time_;
  // Set to true if all annotated visits were fetched. It's set in the DB thread
  // and used in the main thread to determine `continuation_end_time`.
  bool exhausted_history_{false};
  // The options to use when fetching annotated visits. It's updated on each
  // fetch in the DB thread and used in the main thread to determine
  // `continuation_end_time`.
  history::QueryOptions options_;
  // This task stops fetching days of History once we've hit this soft cap,
  // which is controlled by the UI. Note there is a separate
  // parameter-controlled hard cap to prevent OOM errors if a single day has too
  // many visits.
  size_t visit_soft_cap_;
  // Persisted visits retrieved from the history DB thread and returned through
  // the callback on the main thread.
  std::vector<history::AnnotatedVisit> annotated_visits_;
  // The callback called on the main thread on completion.
  Callback callback_;
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_DB_TASKS_H_
