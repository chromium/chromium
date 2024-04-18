// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/history_clusters_db_tasks.h"

#include <algorithm>

#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/history_clusters_types.h"
#include "components/history_clusters/core/history_clusters_util.h"

namespace history_clusters {

// static
base::Time GetAnnotatedVisitsToCluster::GetBeginTimeOnDayBoundary(
    base::Time time) {
  DCHECK(!time.is_null());
  // Subtract 16 hrs. Chosen to be halfway between boundaries; i.e. 4pm is 12
  // hrs from 4am. This guarantees fetching at least 12 hrs of visits regardless
  // of whether iterating recent or oldest visits first.
  time -= base::Hours(16);
  time = time.LocalMidnight();
  time += base::Hours(4);
  return time;
}

GetAnnotatedVisitsToCluster::GetAnnotatedVisitsToCluster(
    IncompleteVisitMap incomplete_visit_map,
    base::Time begin_time_limit,
    QueryClustersContinuationParams continuation_params,
    bool recent_first,
    int days_of_clustered_visits,
    bool recluster,
    Callback callback)
    : incomplete_visit_map_(incomplete_visit_map),
      begin_time_limit_(
          std::max(begin_time_limit, base::Time::Now() - base::Days(90))),
      continuation_params_(continuation_params),
      recent_first_(recent_first),
      days_of_clustered_visits_(days_of_clustered_visits),
      recluster_(recluster),
      callback_(std::move(callback)) {
  // Callers shouldn't ask for more visits if they've been exhausted.
  DCHECK(!continuation_params.exhausted_unclustered_visits);
  DCHECK_GE(days_of_clustered_visits_, 0);
}

GetAnnotatedVisitsToCluster::~GetAnnotatedVisitsToCluster() = default;

bool GetAnnotatedVisitsToCluster::RunOnDBThread(
    history::HistoryBackend* backend,
    history::HistoryDatabase* db) {
  base::ElapsedThreadTimer query_visits_timer;

  // Because `base::Time::Now()` may change during the async history request,
  // and because determining whether history was exhausted depends on whether
  // the query reached `Now()`, `now` tracks `Now()` at the time the query
  // options were created.
  const auto now = base::Time::Now();

  // It's very unlikely for `now == begin_time_limit_`, but it's theoretically
  // possible if e.g. the keyword cooldown is set to 0ms, and it took 0ms from
  // initiating the `GetAnnotatedVisitsToCluster()` to reach here.
  if (now == begin_time_limit_) {
    continuation_params_.exhausted_unclustered_visits = true;
    continuation_params_.exhausted_all_visits = true;
  }

  history::QueryOptions options;
  // Accumulate 1 day at a time of visits to avoid breaking up clusters.
  while (annotated_visits_.empty() &&
         !continuation_params_.exhausted_unclustered_visits) {
    options = GetHistoryQueryOptions(backend, now);
    DCHECK(!options.begin_time.is_null());
    DCHECK(!options.end_time.is_null());
    DCHECK(options.begin_time != options.end_time);
    bool limited_by_max_count = AddUnclusteredVisits(backend, db, options);
    AddIncompleteVisits(backend, options.begin_time, options.end_time);
    IncrementContinuationParams(options, limited_by_max_count, now);
  }
  AddClusteredVisits(backend, db, options.begin_time);

  base::UmaHistogramTimes(
      "History.Clusters.Backend.QueryAnnotatedVisits.ThreadTime",
      query_visits_timer.Elapsed());

  return true;
}

history::QueryOptions GetAnnotatedVisitsToCluster::GetHistoryQueryOptions(
    history::HistoryBackend* backend,
    base::Time now) {
  history::QueryOptions options;

  // History Clusters wants a complete navigation graph and internally handles
  // de-duplication.
  options.duplicate_policy = history::QueryOptions::KEEP_ALL_DUPLICATES;

  // We hard cap at `options.max_count` which is enforced at the database level
  // to avoid any request getting too many visits causing OOM errors. See
  // https://crbug.com/1262016.
  options.max_count =
      GetConfig().max_visits_to_cluster - annotated_visits_.size();

  // Determine the begin & end times.
  // 1st, set `continuation_time`, either from `continuation_params_`for
  // continuation requests or computed for initial requests.
  base::Time continuation_time;
  if (continuation_params_.is_continuation) {
    continuation_time = continuation_params_.continuation_time;
  } else if (recent_first_) {
    continuation_time = now;
  } else {
    continuation_time =
        std::max(backend->FindMostRecentClusteredTime(), begin_time_limit_);
  }

  // 2nd, derive the other boundary, approximately 1 day before or after
  // `continuation_time`, depending on `recent_first`, and rounded to a day
  // boundary.
  if (recent_first_) {
    options.begin_time = GetBeginTimeOnDayBoundary(continuation_time);
    options.end_time = continuation_time;
  } else {
    options.begin_time = continuation_time;
    options.end_time =
        GetBeginTimeOnDayBoundary(continuation_time) + base::Days(2);
  }

  // 3rd, lastly, make sure the times don't surpass `begin_time_limit_` or
  // `now`.
  options.begin_time = std::clamp(options.begin_time, begin_time_limit_, now);
  options.end_time = std::clamp(options.end_time, begin_time_limit_, now);
  options.visit_order = recent_first_
                            ? history::QueryOptions::VisitOrder::RECENT_FIRST
                            : history::QueryOptions::VisitOrder::OLDEST_FIRST;

  return options;
}

bool GetAnnotatedVisitsToCluster::AddUnclusteredVisits(
    history::HistoryBackend* backend,
    history::HistoryDatabase* db,
    history::QueryOptions options) {
  bool limited_by_max_count = false;

  for (const auto& visit : backend->GetAnnotatedVisits(
           options, /*compute_redirect_chain_start_properties=*/true,
           /*get_unclustered_visits_only=*/false, &limited_by_max_count)) {
    // TODO(crbug.com/41492963): Consider changing `get_unclustered_visits_only`
    // above to true, and getting rid of the `exhausted_unclustered_visits`
    // parameter setting below.
    const bool is_clustered =
        !recluster_
            ? db->GetClusterIdContainingVisit(visit.visit_row.visit_id) > 0
            : false;
    if (is_clustered && recent_first_)
      continuation_params_.exhausted_unclustered_visits = true;

    if (is_clustered) {
      continue;
    }

    annotated_visits_.push_back(std::move(visit));
  }

  return limited_by_max_count;
}

void GetAnnotatedVisitsToCluster::AddIncompleteVisits(
    history::HistoryBackend* backend,
    base::Time begin_time,
    base::Time end_time) {
  // Now we have enough visits for clustering, add all incomplete visits
  // between the current `options.begin_time` and `original_end_time`, as
  // otherwise they will be mysteriously missing from the Clusters UI. They
  // haven't recorded the page end metrics yet, but that's fine. Filter
  // incomplete visits to those that have a `url_row`, have a `visit_row`, and
  // match `options`.
  for (const auto& item : incomplete_visit_map_) {
    auto& incomplete_visit_context_annotations = item.second;
    // Discard incomplete visits that don't have a `url_row` and `visit_row`.
    // It's possible that the `url_row` and `visit_row` will be available
    // before they're needed (i.e. before
    // `GetAnnotatedVisitsToCluster::RunOnDBThread()`). But since it'll only
    // have a copy of the incomplete context annotations, the copy won't have
    // the fields updated. A weak ptr won't help since it can't be accessed on
    // different threads. A `scoped_refptr` could work. However, only very
    // recently opened tabs won't have the rows set, so we don't bother using
    // `scoped_refptr`s.
    if (!incomplete_visit_context_annotations.status.history_rows)
      continue;

    // Discard incomplete visits outside the time bounds of the actual visits
    // we fetched, accounting for the fact that we may have been limited by
    // `options.max_count`.
    const auto& visit_time =
        incomplete_visit_context_annotations.visit_row.visit_time;
    if (visit_time < begin_time || visit_time >= end_time)
      continue;

    // Discard any incomplete visits that were already fetched from History.
    // This can happen when History finishes writing the rows after we snapshot
    // the incomplete visits at the beginning of this task.
    // https://crbug.com/1252047.
    history::VisitID visit_id =
        incomplete_visit_context_annotations.visit_row.visit_id;
    if (base::Contains(annotated_visits_, visit_id, [](const auto& visit) {
          return visit.visit_row.visit_id;
        })) {
      continue;
    }

    // Discard any incomplete visits that are not visible to the user.
    if (!IsTransitionUserVisible(
            incomplete_visit_context_annotations.visit_row.transition)) {
      continue;
    }

    // Compute `referring_visit_of_redirect_chain_start` for each incomplete
    // visit.
    const auto& first_redirect = backend->GetRedirectChainStart(
        incomplete_visit_context_annotations.visit_row);

    // Compute `visit_source` for each incomplete visit.
    history::VisitSource visit_source;
    backend->GetVisitSource(
        incomplete_visit_context_annotations.visit_row.visit_id, &visit_source);

    annotated_visits_.push_back(
        {incomplete_visit_context_annotations.url_row,
         incomplete_visit_context_annotations.visit_row,
         incomplete_visit_context_annotations.context_annotations,
         // TODO(tommycli): Add content annotations.
         {},
         first_redirect.referring_visit,
         first_redirect.opener_visit,
         visit_source});
  }
}

void GetAnnotatedVisitsToCluster::IncrementContinuationParams(
    history::QueryOptions options,
    bool limited_by_max_count,
    base::Time now) {
  continuation_params_.is_continuation = true;

  // If `limited_by_max_count` is true, `annotated_visits_` "shouldn't" be
  // empty. But it actually can be if a visit's URL is missing from the URL
  // table. `limited_by_max_count` is set before visits are filtered to
  // those whose URL is found.
  // TODO(manukh): We shouldn't skip the day's remaining visits when
  //  `annotated_visits_` is empty.
  if (limited_by_max_count && !annotated_visits_.empty()) {
    continuation_params_.continuation_time =
        annotated_visits_.back().visit_row.visit_time;
    continuation_params_.is_partial_day = true;
  } else {
    DCHECK(!continuation_params_.exhausted_unclustered_visits || recent_first_);
    // Prepare `continuation_time` for the next day of visits. It will include
    // all unclustered visits iterated. Except, if `exhausted_unclustered_visits
    // is true, which is only possible if `recent_first_` is true and it just
    // reached the clustering boundary, then prepare `continuation_time` to
    // re-iterate the last iterated day, as the to include the clustered visits
    // of that day.
    continuation_params_.continuation_time =
        (recent_first_ && !continuation_params_.exhausted_unclustered_visits)
            ? options.begin_time
            : options.end_time;
    continuation_params_.is_partial_day = false;

    // We've exhausted history if we've reached `begin_time_limit_` (bound to be
    // at most 90 days old) or `Now()`. This does not necessarily mean we've
    // added all visits; e.g. `begin_time_limit_` can be more recent than 90
    // days ago or the initial `continuation_end_time_` could have been older
    // than now.
    if ((continuation_params_.continuation_time <= begin_time_limit_ &&
         recent_first_) ||
        (continuation_params_.continuation_time >= now && !recent_first_)) {
      continuation_params_.exhausted_unclustered_visits = true;
      continuation_params_.exhausted_all_visits = true;
    }
  }
}

void GetAnnotatedVisitsToCluster::AddClusteredVisits(
    history::HistoryBackend* backend,
    history::HistoryDatabase* db,
    base::Time unclustered_begin_time) {
  if (annotated_visits_.empty() ||
      annotated_visits_.size() >=
          static_cast<size_t>(GetConfig().max_visits_to_cluster) ||
      days_of_clustered_visits_ == 0) {
    return;
  }

  // Get the clusters within `days_of_clustered_visits_` days older than the
  // unclustered visits.
  const auto cluster_ids = db->GetMostRecentClusterIds(
      unclustered_begin_time - base::Days(days_of_clustered_visits_),
      base::Time::Max(), 1000);

  // If we found a cluster and are iterating recent_first_, then we've reached
  // the cluster threshold and have no more unclustered visits remaining.
  if (!cluster_ids.empty() && recent_first_)
    continuation_params_.exhausted_unclustered_visits = true;

  // Add the clustered visits, adding 1 cluster at a time so that partial
  // clusters aren't added.
  for (const auto cluster_id : cluster_ids) {
    const auto visit_ids_of_cluster = db->GetVisitIdsInCluster(cluster_id);
    if (annotated_visits_.size() + visit_ids_of_cluster.size() >
        static_cast<size_t>(GetConfig().max_visits_to_cluster))
      break;
    cluster_ids_.push_back(cluster_id);
    base::ranges::move(backend->ToAnnotatedVisitsFromIds(
                           visit_ids_of_cluster,
                           /*compute_redirect_chain_start_properties=*/true),
                       std::back_inserter(annotated_visits_));
  }
}

void GetAnnotatedVisitsToCluster::DoneRunOnMainThread() {
  std::move(callback_).Run(cluster_ids_, annotated_visits_,
                           continuation_params_);
}

}  // namespace history_clusters
