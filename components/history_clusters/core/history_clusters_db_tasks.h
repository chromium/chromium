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

  // For a given `end_time`, this returns an appropriate beginning time
  // designed to avoid breaking up internet browsing sessions. In the morning,
  // it returns 4AM the previous day. In the afternoon, it returns 4AM today.
  static base::Time GetBeginTimeOnDayBoundary(base::Time end_time);

  GetAnnotatedVisitsToCluster(
      HistoryClustersService::IncompleteVisitMap incomplete_visit_map,
      base::Time begin_time,
      base::Time end_time,
      Callback callback);
  ~GetAnnotatedVisitsToCluster() override;

  // history::HistoryDBTask:
  bool RunOnDBThread(history::HistoryBackend* backend,
                     history::HistoryDatabase* db) override;
  void DoneRunOnMainThread() override;

 private:
  // Helper for `RunOnDBThread()` that adds complete but unclustered visits
  // from `backend` to `annotated_visits_`. Returns whether the visits were
  // limited by `options.max_count`.
  bool AddUnclusteredVisits(history::HistoryBackend* backend,
                            history::QueryOptions options);

  // Helper for `RunOnDBThread()` that adds incomplete visits from
  // `incomplete_visit_map_` to `annotated_visits_`.
  void AddIncompleteVisits(history::HistoryBackend* backend);

  // Helper for `RunOnDBThread()` that removes synced visits from
  // `annotated_visits_`.
  void RemoveVisitsFromSync();

  // Incomplete visits that have history rows and are withing the time frame of
  // the completed visits fetched will be appended to the annotated visits
  // returned for clustering. It's used in the DB thread as each filtered visit
  // will need to fetch its `referring_visit_of_redirect_chain_start`.
  HistoryClustersService::IncompleteVisitMap incomplete_visit_map_;
  // The lower bound of the begin times used in the history requests for
  // completed visits. This is a lower bound time of all the visits fetched,
  // though the visit count cap may be reached before we've queried all the way
  // to `begin_time_limit_`.
  base::Time begin_time_limit_;
  // The end time used in the initial history request for completed visits.
  // This is the upper bound time of all the visits fetched.
  base::Time original_end_time_;
  // The end time used to continue the query onto the "next page".
  // This is the lower bound time of all the visits fetched.
  base::Time continuation_end_time_;
  // True if we have exhausted history up to `begin_time_limit_` or all of
  // History; i.e., we didn't hit the visit count cap.
  bool exhausted_history_ = false;
  // Persisted visits retrieved from the history DB thread and returned through
  // the callback on the main thread.
  std::vector<history::AnnotatedVisit> annotated_visits_;
  // The callback called on the main thread on completion.
  Callback callback_;
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_DB_TASKS_H_
