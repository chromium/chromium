// Copyright 2021 The Chromium Authors. All rights reserved.
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

namespace history_clusters {

// Is the transition user-visible.
bool IsTransitionUserVisible(int32_t transition) {
  ui::PageTransition page_transition = ui::PageTransitionFromInt(transition);
  return (ui::PAGE_TRANSITION_CHAIN_END & transition) != 0 &&
         ui::PageTransitionIsMainFrame(page_transition) &&
         !ui::PageTransitionCoreTypeIs(page_transition,
                                       ui::PAGE_TRANSITION_KEYWORD_GENERATED);
}

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
    base::Time begin_time,
    QueryClustersContinuationParams continuation_params,
    bool recent_first,
    Callback callback)
    : incomplete_visit_map_(incomplete_visit_map),
      begin_time_limit_(
          std::max(begin_time, base::Time::Now() - base::Days(90))),
      continuation_params_(continuation_params),
      recent_first_(recent_first),
      callback_(std::move(callback)) {
  // Callers shouldn't ask for more visits if they've been exhausted.
  DCHECK(!continuation_params.exhausted_history);
}

GetAnnotatedVisitsToCluster::~GetAnnotatedVisitsToCluster() = default;

bool GetAnnotatedVisitsToCluster::RunOnDBThread(
    history::HistoryBackend* backend,
    history::HistoryDatabase* db) {
  base::ElapsedThreadTimer query_visits_timer;

  // The end time used in the initial history request for completed visits.
  // This is the upper bound time of all the visits fetched. Used later to add
  // incomplete visits from the same time range we scanned for completed visits.
  // Cached here as `continuation_params` will be updated after each history
  // request.
  base::Time original_end_time = continuation_params_.continuation_time;

  history::QueryOptions options;
  // Accumulate 1 day at a time of visits to avoid breaking up clusters.

  while (annotated_visits_.empty() && !continuation_params_.is_done) {
    // Because `base::Time::Now()` may change during the async history request,
    // and because determining whether history was exhausted depends on whether
    // the query reached `Now()`, `now` tracks `Now()` at the time the query
    // options were created.
    const auto now = base::Time::Now();

    options = GetHistoryQueryOptions(backend, now);
    if (options.begin_time == options.end_time)
      break;
    // Tack on all the newly fetched visits onto our accumulator vector.
    bool limited_by_max_count = AddUnclusteredVisits(backend, options);
    IncrementContinuationParams(options, limited_by_max_count, now);
  }

  AddIncompleteVisits(backend, continuation_params_.continuation_time,
                      original_end_time);

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
  if (continuation_params_.is_continuation)
    continuation_time = continuation_params_.continuation_time;
  else if (recent_first_)
    continuation_time = now;
  else
    continuation_time = backend->FindMostRecentClusteredTime();

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
    history::QueryOptions options) {
  bool limited_by_max_count = false;

  for (const auto& visit :
       backend->GetAnnotatedVisits(options, &limited_by_max_count)) {
    // Filter out visits from sync.
    // TODO(manukh): Consider allowing the clustering backend to handle sync
    //  visits.
    if (visit.source != history::SOURCE_SYNCED)
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
    if ((!begin_time.is_null() && visit_time < begin_time) ||
        (!end_time.is_null() && visit_time >= end_time)) {
      continue;
    }

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
    continuation_params_.continuation_time =
        recent_first_ ? options.begin_time : options.end_time;
    continuation_params_.is_partial_day = false;

    // We've exhausted history if we've reached `begin_time_limit_` (bound to be
    // at most 90 days old) or `Now()`. This does not necessarily mean we've
    // added all visits; e.g. `begin_time_limit_` can be more recent than 90
    // days ago or the initial `continuation_end_time_` could have been older
    // than now.
    if (continuation_params_.continuation_time <= begin_time_limit_ ||
        continuation_params_.continuation_time >= now) {
      continuation_params_.exhausted_history = true;
      continuation_params_.is_done = true;
    }
  }
}

void GetAnnotatedVisitsToCluster::DoneRunOnMainThread() {
  std::move(callback_).Run(annotated_visits_, continuation_params_);
}

}  // namespace history_clusters
