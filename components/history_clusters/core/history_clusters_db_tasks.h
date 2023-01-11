// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_DB_TASKS_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_DB_TASKS_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "components/history_clusters/core/history_clusters_types.h"

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
  using Callback = base::OnceCallback<void(
      std::vector<int64_t> cluster_ids,
      std::vector<history::AnnotatedVisit> annotated_visits,
      QueryClustersContinuationParams continuation_params)>;

  // For a given `end_time`, this returns an appropriate beginning time
  // designed to avoid breaking up internet browsing sessions. Before 4PM, it
  // returns 4AM the previous day. After 4PM, it returns 4AM today.
  static base::Time GetBeginTimeOnDayBoundary(base::Time time);

  GetAnnotatedVisitsToCluster(
      IncompleteVisitMap incomplete_visit_map,
      base::Time begin_time_limit,
      QueryClustersContinuationParams continuation_params,
      bool recent_first,
      int days_of_clustered_visits,
      bool recluster,
      Callback callback);
  ~GetAnnotatedVisitsToCluster() override;

  // history::HistoryDBTask:
  bool RunOnDBThread(history::HistoryBackend* backend,
                     history::HistoryDatabase* db) override;
  void DoneRunOnMainThread() override;

 private:
  // Helper for `RunOnDBThread()` that generates the `QueryOptions` for each
  // history request. Because `base::Time::Now()` may change during the async
  // history request, and because determining whether history was exhausted
  // depends on whether the query reached `Now()`, `now` is set when this
  // function is called and shared with `IncrementContinuationParams()`.
  history::QueryOptions GetHistoryQueryOptions(history::HistoryBackend* backend,
                                               base::Time now);

  // Helper for `RunOnDBThread()` that adds complete but unclustered visits
  // from `backend` to `annotated_visits_`. Returns whether the visits were
  // limited by `options.max_count`.
  bool AddUnclusteredVisits(history::HistoryBackend* backend,
                            history::HistoryDatabase* db,
                            history::QueryOptions options);

  // Helper for `RunOnDBThread()` that adds incomplete visits from
  // `incomplete_visit_map_` to `annotated_visits_`.
  void AddIncompleteVisits(history::HistoryBackend* backend,
                           base::Time begin_time,
                           base::Time end_time);

  // Helper for `RunOnDBThread()` that updates `continuation_params_` after each
  // history request preparing it for the next call. See
  // `GetHistoryQueryOptions()`'s comment regarding `now`.
  void IncrementContinuationParams(history::QueryOptions options,
                                   bool limited_by_max_count,
                                   base::Time now);

  // Helper for `RunOnDbThread()` that adds clustered visits from `backend` and
  // `db` to `annotated_visits_`.
  void AddClusteredVisits(history::HistoryBackend* backend,
                          history::HistoryDatabase* db,
                          base::Time unclustered_begin_time);

  // Incomplete visits that have history rows and are withing the time frame of
  // the completed visits fetched will be appended to the annotated visits
  // returned for clustering. It's used in the DB thread as each filtered visit
  // will need to fetch its `referring_visit_of_redirect_chain_start`.
  IncompleteVisitMap incomplete_visit_map_;

  // The lower bound of the begin times used in the history requests for
  // completed visits. This is a lower bound time of all the visits fetched,
  // though the visit count cap may be reached before we've queried all the way
  // to `begin_time_limit_`.
  base::Time begin_time_limit_;

  // The current continuation state representing what's already been queried and
  // where the next query should pick up. Initially set in the constructor and
  // updated after each history request. The final state will be returned to
  // `callback_` to be used in the next query task.
  QueryClustersContinuationParams continuation_params_;

  // Whether to begin with the most recent visits and iterate towards older
  // visits, or vice versa. Since persistent clustering begins with older
  // visits, clustered visits will be older than unclustered visits (except
  // unclustered sync visits). Therefore, when `recent_first_` is true,
  // unclustered visits are iterated 1st and
  // `continuation_params_.exhausted_unclustered_visits` will be set true before
  // (or simultaneously) with `.exhausted_all_visits`. When `recent_first_` is
  // false, both will be set true only when all visits until now have been
  // iterated; i.e. `.exhausted_unclustered_visits == .exhausted_all_visits`.
  bool recent_first_ = true;

  // How many days of clustered visits to include. When 0, will return only 1
  // day of unclustered annotated visits. Otherwise, will additionally return
  // clustered visits up to `days_of_clustered_visits_` days older than the
  // unclustered visits; i.e. clustered visits newer than
  // `days_of_clustered_visits_` before the last `QueryOptions::begin_time`.
  // Works the same regardless of `recent_first_`. This is useful to re-cluster
  // already clustered visits with unclustered visits to allow existing clusters
  // to grow rather than be split up arbitrarily at day boundaries.
  int days_of_clustered_visits_;

  // If true, forces reclustering as if `persist_clusters_in_history_db` were
  // false.
  bool recluster_;

  // The clusters whose visits were returned. Any cluster included will have all
  // its visits included; i.e. won't return partial clusters. Retrieved from the
  // history DB thread and returned through the callback on the main thread.
  std::vector<int64_t> cluster_ids_;

  // Persisted visits retrieved from the history DB thread and returned through
  // the callback on the main thread.
  std::vector<history::AnnotatedVisit> annotated_visits_;

  // The callback called on the main thread on completion.
  Callback callback_;
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_DB_TASKS_H_
