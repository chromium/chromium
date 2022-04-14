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
#include "components/history_clusters/core/features.h"

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
    base::Time end_time) {
  // Conventionally, `end_time` being null means to fetch History starting from
  // right now, so we explicitly convert that to `Now()` here.
  base::Time begin_time = end_time.is_null() ? base::Time::Now() : end_time;
  begin_time -= base::Hours(12);
  begin_time = begin_time.LocalMidnight();
  begin_time += base::Hours(4);
  return begin_time;
}

GetAnnotatedVisitsToCluster::GetAnnotatedVisitsToCluster(
    HistoryClustersService::IncompleteVisitMap incomplete_visit_map,
    base::Time begin_time,
    base::Time end_time,
    Callback callback)
    : incomplete_visit_map_(incomplete_visit_map),
      begin_time_limit_(
          std::max(begin_time, base::Time::Now() - base::Days(90))),
      original_end_time_(end_time),
      continuation_end_time_(end_time),
      callback_(std::move(callback)) {}

GetAnnotatedVisitsToCluster::~GetAnnotatedVisitsToCluster() = default;

bool GetAnnotatedVisitsToCluster::RunOnDBThread(
    history::HistoryBackend* backend,
    history::HistoryDatabase* db) {
  base::ElapsedThreadTimer query_visits_timer;

  history::QueryOptions options;

  // History Clusters wants a complete navigation graph and internally handles
  // de-duplication.
  options.duplicate_policy = history::QueryOptions::KEEP_ALL_DUPLICATES;

  // Accumulate 1 day at a time of visits to avoid breaking up clusters.
  // We hard cap at `options.max_count` which is enforced at the database level
  // to avoid any one day blasting past the hard cap, causing OOM errors.
  while (annotated_visits_.empty() && !exhausted_history_) {
    // Provide a parameter-controlled hard-cap of the max visits to fetch.
    // Note in most cases we stop fetching visits far before reaching this
    // number. This is to prevent OOM errors. See https://crbug.com/1262016.
    options.max_count =
        GetConfig().max_visits_to_cluster - annotated_visits_.size();

    // Bound visits by `original_end_time_` and `begin_time_limit_`, fetching
    // the more recent visits 1st.
    options.end_time = continuation_end_time_;
    options.begin_time = std::max(begin_time_limit_,
                                  GetBeginTimeOnDayBoundary(options.end_time));

    // Tack on all the newly fetched visits onto our accumulator vector.
    bool limited_by_max_count = AddUnclusteredVisits(backend, options);

    // If we didn't get enough visits, ask for another day's worth from History
    // and call this method again when done.
    // If `limited_by_max_count` is true, `annotated_visits_` "shouldn't" be
    // empty. But it actually can be if a visit's URL is missing from the URL
    // table. `limited_by_max_count` is set before visits are filtered to
    // those whose URL is found.
    if (limited_by_max_count && !annotated_visits_.empty()) {
      continuation_end_time_ = annotated_visits_.back().visit_row.visit_time;
    } else {
      continuation_end_time_ = options.begin_time;
    }

    // TODO(tommycli): Connect this to History's limit defined internally in
    //  components/history.
    // `exhausted_history_` is true if we've reached `begin_time_limit_` (bound
    // to be at most 90 days old). This does not necessarily mean we've added
    // all visits; e.g. `begin_time_limit_` can be more recent than 90 days ago
    // or `original_end_time_` can be older than now.
    exhausted_history_ =
        !limited_by_max_count && continuation_end_time_ <= begin_time_limit_;
  }

  AddIncompleteVisits(backend);

  RemoveVisitsFromSync();

  base::UmaHistogramTimes(
      "History.Clusters.Backend.QueryAnnotatedVisits.ThreadTime",
      query_visits_timer.Elapsed());

  return true;
}

bool GetAnnotatedVisitsToCluster::AddUnclusteredVisits(
    history::HistoryBackend* backend,
    history::QueryOptions options) {
  bool limited_by_max_count = false;
  base::ranges::move(
      backend->GetAnnotatedVisits(options, &limited_by_max_count),
      std::back_inserter(annotated_visits_));
  return limited_by_max_count;
}

void GetAnnotatedVisitsToCluster::AddIncompleteVisits(
    history::HistoryBackend* backend) {
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
    if ((!continuation_end_time_.is_null() &&
         visit_time < continuation_end_time_) ||
        (!original_end_time_.is_null() && visit_time >= original_end_time_)) {
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

void GetAnnotatedVisitsToCluster::RemoveVisitsFromSync() {
  // Filter out visits from sync.
  // TODO(manukh): Consider allowing the clustering backend to handle sync
  //  visits.
  annotated_visits_.erase(
      base::ranges::remove_if(annotated_visits_,
                              [](const auto& annotated_visit) {
                                return annotated_visit.source ==
                                       history::SOURCE_SYNCED;
                              }),
      annotated_visits_.end());
}

void GetAnnotatedVisitsToCluster::DoneRunOnMainThread() {
  // Don't give a continuation end time if we exhausted all of History.
  base::Time continuation_end_time_result;
  if (!exhausted_history_)
    continuation_end_time_result = continuation_end_time_;

  std::move(callback_).Run(annotated_visits_, continuation_end_time_result);
}

}  // namespace history_clusters
