// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/history_url_visit_data_fetcher.h"

#include <map>
#include <utility>

#include "base/containers/contains.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"
#include "components/visited_url_ranking/internal/url_visit_util.h"
#include "components/visited_url_ranking/public/fetch_result.h"
#include "url/gurl.h"

namespace visited_url_ranking {

using Source = URLVisit::Source;
using URLVisitVariant = URLVisitAggregate::URLVisitVariant;

HistoryURLVisitDataFetcher::HistoryURLVisitDataFetcher(
    base::WeakPtr<history::HistoryService> history_service)
    : history_service_(history_service) {}

HistoryURLVisitDataFetcher::~HistoryURLVisitDataFetcher() = default;

void HistoryURLVisitDataFetcher::FetchURLVisitData(
    const FetchOptions& options,
    FetchResultCallback callback) {
  history::QueryOptions query_options;
  query_options.begin_time = options.begin_time;
  query_options.duplicate_policy =
      history::QueryOptions::DuplicateHandling::KEEP_ALL_DUPLICATES;

  if (history_service_) {
    history_service_->GetAnnotatedVisits(
        query_options,
        /*compute_redirect_chain_start_properties=*/true,
        /*get_unclustered_visits_only=*/false,
        base::BindOnce(&HistoryURLVisitDataFetcher::OnGotAnnotatedVisits,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       options.fetcher_sources.at(Fetcher::kHistory)),
        &task_tracker_);
    return;
  }

  std::move(callback).Run({FetchResult::Status::kError, {}});
}

void HistoryURLVisitDataFetcher::OnGotAnnotatedVisits(
    FetchResultCallback callback,
    FetchOptions::FetchSources requested_fetch_sources,
    std::vector<history::AnnotatedVisit> annotated_visits) {
  std::map<std::string, URLVisitAggregate::HistoryData> url_annotations;
  for (auto& annotated_visit : annotated_visits) {
    // The `originator_cache_guid` field is only set for foreign session visits.
    Source current_visit_source =
        annotated_visit.visit_row.originator_cache_guid.empty()
            ? Source::kLocal
            : Source::kForeign;
    if (!base::Contains(requested_fetch_sources, current_visit_source)) {
      continue;
    }

    auto url_key = ComputeURLMergeKey(annotated_visit.url_row.url());
    if (url_annotations.find(url_key) == url_annotations.end()) {
      // `GetAnnotatedVisits` returns a reverse-chronological sorted list of
      // annotated visits, thus, the first visit in the vector is the most
      // recent visit for a given URL.
      url_annotations.emplace(url_key, std::move(annotated_visit));
    } else {
      auto& history = url_annotations.at(url_key);
      history.visit_count += 1;
      history.total_foreground_duration +=
          annotated_visit.context_annotations.total_foreground_duration;
      // TODO(crbug.com/330580109): Add `in_cluster`, dismiss/done `status`
      // signals.
    }
  }

  std::map<URLMergeKey, URLVisitVariant> url_visit_variant_map;
  std::transform(
      std::make_move_iterator(url_annotations.begin()),
      std::make_move_iterator(url_annotations.end()),
      std::inserter(url_visit_variant_map, url_visit_variant_map.end()),
      [](auto kv) { return std::make_pair(kv.first, std::move(kv.second)); });

  std::move(callback).Run(
      {FetchResult::Status::kSuccess, std::move(url_visit_variant_map)});
}

}  // namespace visited_url_ranking
