// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/browsing_history_service.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "base/values.h"
#include "components/history/core/browser/browsing_history_driver.h"
#include "components/history/core/browser/features.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_types.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/sync/protocol/history_delete_directive_specifics.pb.h"

namespace history {

namespace {

// The amount of time to wait for a response from the WebHistoryService.
constexpr int kWebHistoryTimeoutSeconds = 3;

QueryOptions OptionsWithEndTime(QueryOptions original_options,
                                base::Time end_time) {
  QueryOptions options(original_options);
  options.end_time = end_time;
  return options;
}

// The status of the result from a particular history source.
enum QuerySourceStatus {
  // Not a continuation and no response yet.
  UNINITIALIZED,
  // Could not query the particular source.
  NO_DEPENDENCY,
  // Only used for web, when we stop waiting for a response due to timeout.
  TIMED_OUT,
  // Only used for remote, response was error or empty.
  FAILURE,
  // Successfully retrieved results, but there are more left.
  MORE_RESULTS,
  // Successfully retrieved results and we reached the end of results.
  REACHED_BEGINNING,
};

bool CanRetry(QuerySourceStatus status) {
  // TODO(skym): Should we be retrying on FAILURE?
  return status == UNINITIALIZED || status == MORE_RESULTS ||
         status == FAILURE || status == TIMED_OUT;
}

base::Time OldestTime(
    const std::vector<BrowsingHistoryService::HistoryEntry>& entries) {
  // If the vector is empty, then there is no oldest, so we use the oldest
  // possible time instead.
  if (entries.empty()) {
    return base::Time();
  }

  base::Time best = base::Time::Max();
  for (const BrowsingHistoryService::HistoryEntry& entry : entries) {
    best = std::min(best, entry.time);
  }
  return best;
}

}  // namespace

struct BrowsingHistoryService::QueryHistoryState
    : public base::RefCounted<BrowsingHistoryService::QueryHistoryState> {
  QueryHistoryState() = default;

  std::u16string search_text;
  QueryOptions original_options;

  QuerySourceStatus local_status = UNINITIALIZED;
  // Should always be sorted in reverse chronological order.
  std::vector<HistoryEntry> local_results;
  base::Time local_end_time_for_continuation;

  QuerySourceStatus remote_status = UNINITIALIZED;
  // Should always be sorted in reverse chronological order.
  std::vector<HistoryEntry> remote_results;
  base::Time remote_end_time_for_continuation;

 private:
  friend class base::RefCounted<BrowsingHistoryService::QueryHistoryState>;
  ~QueryHistoryState() = default;
};

BrowsingHistoryService::HistoryEntry::HistoryEntry(
    BrowsingHistoryService::HistoryEntry::EntryType entry_type,
    const GURL& url,
    const std::u16string& title,
    base::Time time,
    const std::string& client_id,
    bool is_search_result,
    const std::u16string& snippet,
    bool blocked_visit,
    const GURL& remote_icon_url_for_uma,
    int visit_count,
    int typed_count,
    bool is_actor_visit,
    std::optional<std::string> app_id)
    : entry_type(entry_type),
      url(url),
      title(title),
      time(time),
      client_id(client_id),
      is_search_result(is_search_result),
      snippet(snippet),
      blocked_visit(blocked_visit),
      remote_icon_url_for_uma(remote_icon_url_for_uma),
      visit_count(visit_count),
      typed_count(typed_count),
      is_actor_visit(is_actor_visit),
      app_id(app_id) {
  all_timestamps[url].insert(time);
}

BrowsingHistoryService::HistoryEntry::HistoryEntry()
    : entry_type(EMPTY_ENTRY), is_search_result(false), blocked_visit(false) {}

BrowsingHistoryService::HistoryEntry::HistoryEntry(const HistoryEntry& other) =
    default;

BrowsingHistoryService::HistoryEntry::~HistoryEntry() = default;

bool BrowsingHistoryService::HistoryEntry::SortByTimeDescending(
    const BrowsingHistoryService::HistoryEntry& entry1,
    const BrowsingHistoryService::HistoryEntry& entry2) {
  return entry1.time > entry2.time;
}

BrowsingHistoryService::QueryResultsInfo::~QueryResultsInfo() = default;

BrowsingHistoryService::BrowsingHistoryService(
    BrowsingHistoryDriver* driver,
    HistoryService* local_history,
    syncer::SyncService* sync_service)
    : BrowsingHistoryService(driver,
                             local_history,
                             sync_service,
                             std::make_unique<base::OneShotTimer>()) {}

BrowsingHistoryService::BrowsingHistoryService(
    BrowsingHistoryDriver* driver,
    HistoryService* local_history,
    syncer::SyncService* sync_service,
    std::unique_ptr<base::OneShotTimer> web_history_timer)
    : web_history_timer_(std::move(web_history_timer)),
      driver_(driver),
      local_history_(local_history),
      sync_service_(sync_service),
      clock_(new base::DefaultClock()) {
  DCHECK(driver_);

  // Get notifications when history is cleared.
  if (local_history_) {
    history_service_observation_.Observe(local_history_.get());
  }

  // Get notifications when web history is deleted.
  WebHistoryService* web_history = driver_->GetWebHistoryService();
  if (web_history) {
    web_history_service_observation_.Observe(web_history);
  } else if (sync_service_) {
    // If `web_history` is not available, it means that history sync is
    // disabled. If `sync_service_` is not null, it means that syncing is
    // possible, and that history sync/web history may become enabled later, so
    // attach start observing. If `sync_service_` is null then we cannot start
    // observing. This is okay because sync will never start for us, for example
    // it may be disabled by flag or we're part of an incognito/guest mode
    // window.
    sync_service_observation_.Observe(sync_service_.get());
  }
}

BrowsingHistoryService::BrowsingHistoryService() = default;

BrowsingHistoryService::~BrowsingHistoryService() {
  query_task_tracker_.TryCancelAll();
  web_history_request_.reset();
}

void BrowsingHistoryService::OnStateChanged(syncer::SyncService* sync) {
  // If the history sync was enabled, start observing WebHistoryService.
  // This method should not be called after we already added the observer.
  WebHistoryService* web_history = driver_->GetWebHistoryService();
  if (web_history) {
    DCHECK(!web_history_service_observation_.IsObserving());
    web_history_service_observation_.Observe(web_history);
    DCHECK(sync_service_observation_.IsObserving());
    sync_service_observation_.Reset();
  }
}

void BrowsingHistoryService::OnSyncShutdown(syncer::SyncService* sync) {
  sync_service_observation_.Reset();
}

void BrowsingHistoryService::WebHistoryTimeout(
    scoped_refptr<QueryHistoryState> state) {
  state->remote_status = TIMED_OUT;

  // Don't reset `web_history_request_` so we can still record histogram.
  // TODO(dubroy): Communicate the failure to the front end.
  if (!query_task_tracker_.HasTrackedTasks()) {
    ReturnResultsToDriver(std::move(state));
  }
}

void BrowsingHistoryService::QueryHistory(const std::u16string& search_text,
                                          const QueryOptions& options) {
  scoped_refptr<QueryHistoryState> state =
      base::MakeRefCounted<QueryHistoryState>();
  state->search_text = search_text;
  state->original_options = options;
  state->local_end_time_for_continuation = options.end_time;
  state->remote_end_time_for_continuation = options.end_time;
  QueryHistoryInternal(std::move(state));
}

void BrowsingHistoryService::QueryHistoryInternal(
    scoped_refptr<QueryHistoryState> state) {
  // Anything in-flight is invalid.
  query_task_tracker_.TryCancelAll();
  web_history_request_.reset();

  bool should_return_results_immediately = true;
  const size_t desired_count =
      static_cast<size_t>(state->original_options.EffectiveMaxCount());

  if (local_history_) {
    if (state->local_results.size() < desired_count &&
        state->local_status != REACHED_BEGINNING) {
      CHECK_NE(state->local_status, NO_DEPENDENCY);
      should_return_results_immediately = false;
      local_history_->QueryHistory(
          state->search_text,
          OptionsWithEndTime(state->original_options,
                             state->local_end_time_for_continuation),
          base::BindOnce(&BrowsingHistoryService::QueryComplete,
                         weak_factory_.GetWeakPtr(), state),
          &query_task_tracker_);
    }
  } else {
    state->local_status = NO_DEPENDENCY;
  }

  WebHistoryService* web_history = driver_->GetWebHistoryService();
  if (web_history) {
    // Test the existence of other forms of browsing history, to display the
    // privacy disclaimer in the UI. This needs to happen independently of
    // whether an actual remote history query is happening (yet).
    driver_->ShouldShowNoticeAboutOtherFormsOfBrowsingHistory(
        sync_service_, web_history,
        base::BindOnce(
            &BrowsingHistoryService::OtherFormsOfBrowsingHistoryQueryComplete,
            weak_factory_.GetWeakPtr()));

    // If necessary, run a WebHistory query for remote history.
    if (ShouldQueryRemote(*state)) {
      // Start a timer with timeout before we make the actual query, otherwise
      // tests get confused when completion callback is run synchronously.
      web_history_timer_->Start(
          FROM_HERE, base::Seconds(kWebHistoryTimeoutSeconds),
          base::BindOnce(&BrowsingHistoryService::WebHistoryTimeout,
                         weak_factory_.GetWeakPtr(), state));

      net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation =
          net::DefinePartialNetworkTrafficAnnotation("web_history_query",
                                                     "web_history_service",
                                                     R"(
            semantics {
              description:
                "If history sync is enabled, this downloads the synced "
                "history from history.google.com."
              trigger:
                "Synced history is downloaded when user opens the history "
                "page, searches on the history page, or scrolls down the "
                "history page to see more results. This is only the case if "
                "the user is signed in and history sync is enabled."
              data:
                "The history query text (or empty strings if all results are "
                "to be fetched), the begin and end timestamps, and the maximum "
                "number of results to be fetched. The request also includes a "
                "version info token to resolve transaction conflicts, and an "
                "OAuth2 token authenticating the user."
            }
            policy {
              chrome_policy {
                SyncDisabled {
                  SyncDisabled: true
                }
              }
            })");
      should_return_results_immediately = false;
      QueryOptions options = OptionsWithEndTime(
          state->original_options, state->remote_end_time_for_continuation);
      if (base::FeatureList::IsEnabled(kHistoryQueryOnlyLocalFirst)) {
        options.max_count = desired_count - state->local_results.size();
        // If no remote results were needed, ShouldQueryRemote() should have
        // returned false and control flow wouldn't reach here.
        CHECK(options.max_count > 0);
      }
      web_history_request_ = web_history->QueryHistory(
          state->search_text, options,
          base::BindOnce(&BrowsingHistoryService::WebHistoryQueryComplete,
                         weak_factory_.GetWeakPtr(), state, clock_->Now()),
          partial_traffic_annotation);
    }
  } else {
    state->remote_status = NO_DEPENDENCY;
    // The notice could not have been shown, because there is no web history.
    has_synced_results_ = false;
    has_other_forms_of_browsing_history_ = false;
  }

  // If we avoid assuming delegated queries are returning asynchronous, then we
  // cannot check tracker/timer, and instead have to rely on local logic for
  // this choice. Note that in unit tests Web History returns synchronously.
  if (should_return_results_immediately) {
    ReturnResultsToDriver(std::move(state));
  }
}

void BrowsingHistoryService::GetAllAppIds() {
  local_history_->GetAllAppIds(
      base::BindOnce(&BrowsingHistoryService::OnGetAllAppIds,
                     weak_factory_.GetWeakPtr()),
      &query_task_tracker_);
}

void BrowsingHistoryService::GetLastVisitToHostBeforeRecentNavigations(
    const std::string& host_name,
    base::OnceCallback<void(base::Time)> callback) {
  base::Time now = base::Time::Now();
  local_history_->GetLastVisitToHost(
      host_name, /*begin_time=*/base::Time(), /*end_time=*/now,
      VisitQuery404sPolicy::kExclude404s,
      base::BindOnce(
          &BrowsingHistoryService::OnLastVisitBeforeRecentNavigationsComplete,
          weak_factory_.GetWeakPtr(), host_name, now, std::move(callback)),
      &query_task_tracker_);
}

void BrowsingHistoryService::OnLastVisitBeforeRecentNavigationsComplete(
    const std::string& host_name,
    base::Time query_start_time,
    base::OnceCallback<void(base::Time)> callback,
    HistoryLastVisitResult result) {
  if (!result.success || result.last_visit.is_null()) {
    std::move(callback).Run(base::Time());
    return;
  }

  base::Time end_time =
      result.last_visit < (query_start_time - base::Minutes(1))
          ? result.last_visit
          : query_start_time - base::Minutes(1);
  local_history_->GetLastVisitToHost(
      host_name, /*begin_time=*/base::Time(), end_time,
      VisitQuery404sPolicy::kExclude404s,
      base::BindOnce(
          &BrowsingHistoryService::OnLastVisitBeforeRecentNavigationsComplete2,
          weak_factory_.GetWeakPtr(), std::move(callback)),
      &query_task_tracker_);
}

void BrowsingHistoryService::OnLastVisitBeforeRecentNavigationsComplete2(
    base::OnceCallback<void(base::Time)> callback,
    HistoryLastVisitResult result) {
  std::move(callback).Run(result.last_visit);
}

void BrowsingHistoryService::RemoveVisits(
    const std::vector<BrowsingHistoryService::HistoryEntry>& items) {
  if (delete_task_tracker_.HasTrackedTasks() || has_pending_delete_request_ ||
      !driver_->AllowHistoryDeletions()) {
    driver_->OnRemoveVisitsFailed();
    return;
  }

  WebHistoryService* web_history = driver_->GetWebHistoryService();
  base::Time now = clock_->Now();
  std::vector<ExpireHistoryArgs> expire_list;
  expire_list.reserve(items.size());

  for (const BrowsingHistoryService::HistoryEntry& entry : items) {
    // In order to ensure that visits will be deleted from the server and other
    // clients (even if they are offline), create a sync delete directive for
    // each visit to be deleted.
    sync_pb::HistoryDeleteDirectiveSpecifics delete_directive;
    sync_pb::GlobalIdDirective* global_id_directive =
        delete_directive.mutable_global_id_directive();
    ExpireHistoryArgs* expire_args = nullptr;

    for (const auto& [url, timestamps] : entry.all_timestamps) {
      // Add every timestamp for every similar or duplicated visit.
      for (base::Time timestamp : timestamps) {
        if (!expire_args) {
          expire_list.resize(expire_list.size() + 1);
          expire_args = &expire_list.back();
          expire_args->SetTimeRangeForOneDay(timestamp);
          expire_args->urls.insert(url);
        }

        // The local visit time is treated as a global ID for the visit.
        global_id_directive->add_global_id(
            timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());
      }
    }

    // Set the start and end time in microseconds since the Unix epoch.
    global_id_directive->set_start_time_usec(
        (expire_args->begin_time - base::Time::UnixEpoch()).InMicroseconds());

    // Delete directives shouldn't have an end time in the future.
    base::Time end_time = std::min(expire_args->end_time, now);

    // -1 because end time in delete directives is inclusive.
    global_id_directive->set_end_time_usec(
        (end_time - base::Time::UnixEpoch()).InMicroseconds() - 1);

    expire_args->restrict_app_id = entry.app_id;

    // TODO(dubroy): Figure out the proper way to handle an error here.
    if (web_history && local_history_) {
      local_history_->ProcessLocalDeleteDirective(delete_directive);
    }
  }

  if (local_history_) {
    local_history_->ExpireHistory(
        expire_list,
        base::BindOnce(&BrowsingHistoryService::RemoveComplete,
                       weak_factory_.GetWeakPtr()),
        &delete_task_tracker_);
  }

  if (web_history) {
    has_pending_delete_request_ = true;
    net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation =
        net::DefinePartialNetworkTrafficAnnotation("web_history_expire",
                                                   "web_history_service",
                                                   R"(
          semantics {
            description:
              "If a user who syncs their browsing history deletes one or more "
              "history item(s), Chrome sends a request to history.google.com "
              "to execute the corresponding deletion serverside."
            trigger:
              "Deleting one or more history items form the history page."
            data:
              "The selected items represented by a URL and timestamp. The "
              "request also includes a version info token to resolve "
              "transaction conflicts, and an OAuth2 token authenticating the "
              "user."
          }
          policy {
            chrome_policy {
              AllowDeletingBrowserHistory {
                AllowDeletingBrowserHistory: false
              }
            }
          })");
    web_history->ExpireHistory(
        expire_list,
        base::BindOnce(&BrowsingHistoryService::RemoveWebHistoryComplete,
                       weak_factory_.GetWeakPtr()),
        partial_traffic_annotation);

    base::UmaHistogramCounts1000(
        "History.RemoveVisitsFromWebHistory.EntryCount", expire_list.size());
  }

  driver_->OnRemoveVisits(expire_list);
}

// static
bool BrowsingHistoryService::ShouldQueryRemote(const QueryHistoryState& state) {
  if (state.remote_status == REACHED_BEGINNING) {
    // Finished with remote history, no point in querying any more.
    return false;
  }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // Actor visits are local-only and user visits should not be queried.
  if (history::IsBrowsingHistoryActorIntegrationM3Enabled() &&
      !state.original_options.include_user_visits) {
    return false;
  }
#endif

  const size_t desired_count =
      static_cast<size_t>(state.original_options.EffectiveMaxCount());
  if (base::FeatureList::IsEnabled(kHistoryQueryOnlyLocalFirst)) {
    if (CanRetry(state.local_status)) {
      // There is more local history to query first, so don't query remote yet.
      return false;
    }
    if (state.local_results.size() + state.remote_results.size() >=
        desired_count) {
      // Already have sufficient results, no need to query more.
      return false;
    }
  } else {
    if (state.remote_results.size() >= desired_count) {
      // Already have sufficient results, no need to query more.
      return false;
    }
  }

  // App-specific history uses the results from the local database only, since
  // the legacy json API service WebHistory relies on can't be updated to
  // process app_id.
  // TODO(crbug.com/460361854): Once migrated to a non-legacy API, also query
  // remote app-specific history.
  if (state.original_options.app_id != kNoAppIdFilter) {
    return false;
  }

  return true;
}

// static
void BrowsingHistoryService::HoldbackAndPartitionResults(
    QueryHistoryState* state,
    const base::Time oldest_local,
    const base::Time oldest_remote,
    std::vector<HistoryEntry>* results) {
  // If the beginning of either source was not reached, that means there are
  // more results from that source, and the other source needs to have its data
  // held back until the former source catches up. This only sends the UI
  // history entries in the correct order. Subsequent continuation requests will
  // get the delayed entries.
  base::Time oldest_allowed = base::Time();
  if (state->local_status == MORE_RESULTS) {
    oldest_allowed = std::max(oldest_allowed, oldest_local);
    state->local_end_time_for_continuation = oldest_local;
  }
  if (state->remote_status == MORE_RESULTS) {
    oldest_allowed = std::max(oldest_allowed, oldest_remote);
    state->remote_end_time_for_continuation = oldest_remote;
  } else if (CanRetry(state->remote_status) && oldest_local != base::Time()) {
    // TODO(skym): It is unclear if this is the best behavior. The UI is going
    // to behave incorrectly if out of order results are received. So to
    // guarantee that doesn't happen, use `oldest_local` for continuation
    // calls. This will result in missing history entries for the failed calls.
    // crbug.com/685866 is related to this problem.
    state->remote_end_time_for_continuation = oldest_local;
  }

  HistoryEntry search_entry;
  search_entry.time = oldest_allowed;
  auto threshold_iter =
      std::upper_bound(results->begin(), results->end(), search_entry,
                       HistoryEntry::SortByTimeDescending);

  // Everything from threshold_iter to results->end() should either be all local
  // or all remote, never a mix.
  if (threshold_iter != results->end()) {
    if (threshold_iter->entry_type == HistoryEntry::LOCAL_ENTRY) {
      state->local_results.assign(std::make_move_iterator(threshold_iter),
                                  std::make_move_iterator(results->end()));
    } else if (threshold_iter->entry_type == HistoryEntry::REMOTE_ENTRY) {
      state->remote_results.assign(std::make_move_iterator(threshold_iter),
                                   std::make_move_iterator(results->end()));
    } else {
      NOTREACHED();
    }
    results->erase(threshold_iter, results->end());
  }
}

void BrowsingHistoryService::MergeDuplicateResults(
    QueryHistoryState* state,
    std::vector<HistoryEntry>* results) {
  DCHECK(state);
  DCHECK(results);
  DCHECK(results->empty());

  // Will be used later to decide if we need to hold back results. This iterates
  // through each entry and makes no assumptions about their ordering.
  base::Time oldest_local = OldestTime(state->local_results);
  base::Time oldest_remote = OldestTime(state->remote_results);

  std::vector<HistoryEntry> sorted;
  sorted.assign(std::make_move_iterator(state->local_results.begin()),
                std::make_move_iterator(state->local_results.end()));
  state->local_results.clear();
  sorted.insert(sorted.end(),
                std::make_move_iterator(state->remote_results.begin()),
                std::make_move_iterator(state->remote_results.end()));
  state->remote_results.clear();
  std::sort(sorted.begin(), sorted.end(), HistoryEntry::SortByTimeDescending);

  // Pre-reserve the size of the new vector. Since we're working with pointers
  // later on, not doing this could lead to the vector being resized and to
  // pointers to invalid locations.
  std::vector<HistoryEntry> deduped;
  deduped.reserve(sorted.size());

  // Maps a URL to the most recent entry on a particular day for
  // non-actor-initiated visits.
  std::map<GURL, HistoryEntry*> non_actor_current_day_entries;
  // Same as above, but for actor-initiated visits.
  std::map<GURL, HistoryEntry*> actor_current_day_entries;

  // Keeps track of the day that `*_current_day_entries` is holding
  // entries for in order to handle removing per-day duplicates.
  base::Time current_day_midnight;

  for (HistoryEntry& entry : sorted) {
    // Reset the list of found URLs when a visit from a new day is encountered.
    if (current_day_midnight != entry.time.LocalMidnight()) {
      non_actor_current_day_entries.clear();
      actor_current_day_entries.clear();
      current_day_midnight = entry.time.LocalMidnight();
    }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    auto& current_day_entries =
        history::IsBrowsingHistoryActorIntegrationM2Enabled() &&
                entry.is_actor_visit
            ? actor_current_day_entries
            : non_actor_current_day_entries;
#else   // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    auto& current_day_entries = non_actor_current_day_entries;
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

    // Keep this visit if it's the first visit to this URL on the current day.
    if (current_day_entries.count(entry.url) == 0) {
      const auto entry_url = entry.url;
      deduped.push_back(std::move(entry));
      current_day_entries[entry_url] = &deduped.back();
    } else {
      // Keep track of the timestamps of all visits to the URL on the same day.
      HistoryEntry* matching_entry = current_day_entries[entry.url];
      // Since this de-duplication logic will only be performed if the grouping
      // is disabled, the entries will only have timestamps for the same URL.
      CHECK_EQ(1u, entry.all_timestamps.size());
      CHECK(entry.all_timestamps.count(entry.url) != 0);
      matching_entry->all_timestamps[entry.url].insert(
          entry.all_timestamps[entry.url].begin(),
          entry.all_timestamps[entry.url].end());

      if (matching_entry->entry_type != entry.entry_type) {
        matching_entry->entry_type = HistoryEntry::COMBINED_ENTRY;
      }

      // Get first non-empty remote icon url.
      if (matching_entry->remote_icon_url_for_uma.is_empty() &&
          !entry.remote_icon_url_for_uma.is_empty()) {
        matching_entry->remote_icon_url_for_uma = entry.remote_icon_url_for_uma;
      }

      // Aggregate visit and typed counts.
      matching_entry->visit_count += entry.visit_count;
      matching_entry->typed_count += entry.typed_count;

      // TODO(crbug.com/481934455): Aggregate all relevant HistoryEntry fields
      // for combined entries.
    }
  }

  HoldbackAndPartitionResults(state, oldest_local, oldest_remote, &deduped);
  *results = std::move(deduped);
}

std::vector<BrowsingHistoryService::HistoryEntry>
BrowsingHistoryService::GroupSimilarVisits(QueryHistoryState* state) {
  CHECK(state);

  // Will be used later to decide if we need to hold back results. This iterates
  // through each entry and makes no assumptions about their ordering.
  base::Time oldest_local = OldestTime(state->local_results);
  base::Time oldest_remote = OldestTime(state->remote_results);

  std::vector<HistoryEntry> sorted;
  sorted.assign(std::make_move_iterator(state->local_results.begin()),
                std::make_move_iterator(state->local_results.end()));
  state->local_results.clear();
  sorted.insert(sorted.end(),
                std::make_move_iterator(state->remote_results.begin()),
                std::make_move_iterator(state->remote_results.end()));
  state->remote_results.clear();
  std::sort(sorted.begin(), sorted.end(), HistoryEntry::SortByTimeDescending);

  // Pre-reserve the size of the new vector. Since we're working with pointers
  // later on, not doing this could lead to the vector being resized and to
  // pointers to invalid locations.
  std::vector<HistoryEntry> grouped;
  grouped.reserve(sorted.size());
  // The GroupingKey consists of a pair of hostname and title.
  using GroupingKey = std::pair<std::string, std::u16string>;

  // Maps the GroupingKey to the most recent entry on a particular day for
  // non-actor-initiated visits.
  std::map<GroupingKey, HistoryEntry*> non_actor_current_day_entries;
  // Same as above, but for actor-initiated visits.
  std::map<GroupingKey, HistoryEntry*> actor_current_day_entries;

  // Keeps track of the day that `*_current_day_entries` is holding
  // entries for in order to handle per-day grouping.
  base::Time current_day_midnight;

  for (HistoryEntry& entry : sorted) {
    // Reset the list of found entries when a visit from a new day is
    // encountered.
    if (current_day_midnight != entry.time.LocalMidnight()) {
      non_actor_current_day_entries.clear();
      actor_current_day_entries.clear();
      current_day_midnight = entry.time.LocalMidnight();
    }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    auto& current_day_entries =
        history::IsBrowsingHistoryActorIntegrationM2Enabled() &&
                entry.is_actor_visit
            ? actor_current_day_entries
            : non_actor_current_day_entries;
#else   // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    auto& current_day_entries = non_actor_current_day_entries;
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

    // TODO(b/481272035): Use the domain name that matches the displayed domain
    // name in the UI.
    GroupingKey key(entry.url.GetHost(), entry.title);
    // Keep this visit if it's the first visit of it's kind on the current day.
    if (current_day_entries.find(key) == current_day_entries.end()) {
      grouped.push_back(std::move(entry));
      current_day_entries[key] = &grouped.back();
    } else {
      HistoryEntry* matching_entry = current_day_entries[key];

      // Merge all timestamps from the current entry into the matching entry.
      // This ensures all visits for similar URLs on the same day are tracked.
      for (const auto& [url, timestamps] : entry.all_timestamps) {
        matching_entry->all_timestamps[url].insert(timestamps.begin(),
                                                   timestamps.end());
      }

      if (matching_entry->entry_type != entry.entry_type) {
        matching_entry->entry_type = HistoryEntry::COMBINED_ENTRY;
      }

      // Get first non-empty remote icon url.
      if (matching_entry->remote_icon_url_for_uma.is_empty() &&
          !entry.remote_icon_url_for_uma.is_empty()) {
        matching_entry->remote_icon_url_for_uma = entry.remote_icon_url_for_uma;
      }

      // Aggregate visit and typed counts.
      matching_entry->visit_count += entry.visit_count;
      matching_entry->typed_count += entry.typed_count;

      // TODO(b/482947398): Aggregate all relevant HistoryEntry fields for
      // combined entries.
    }
  }

  HoldbackAndPartitionResults(state, oldest_local, oldest_remote, &grouped);
  return grouped;
}

void BrowsingHistoryService::QueryComplete(
    scoped_refptr<QueryHistoryState> state,
    QueryResults results) {
  std::vector<HistoryEntry>& output = state->local_results;
  output.reserve(output.size() + results.size());

  for (const auto& page : results) {
    output.emplace_back(
        HistoryEntry::LOCAL_ENTRY, page.url(), page.title(), page.visit_time(),
        std::string(), !state->search_text.empty(), page.snippet().text(),
        page.blocked_visit(), GURL(), page.visit_count(), page.typed_count(),
        page.has_actor_source(), page.app_id());
  }

  state->local_status =
      results.reached_beginning() ? REACHED_BEGINNING : MORE_RESULTS;

  if (base::FeatureList::IsEnabled(kHistoryQueryOnlyLocalFirst) &&
      results.reached_beginning()) {
    // Exhausted the local results; continue querying to get remote results.
    // Start querying at the point where local history ends.
    base::Time expiry_treshold =
        clock_->Now() - base::Days(HistoryBackend::kExpireDaysThreshold);
    if (state->remote_end_time_for_continuation.is_null()) {
      state->remote_end_time_for_continuation = base::Time::Max();
    }
    state->remote_end_time_for_continuation =
        std::min(state->remote_end_time_for_continuation, expiry_treshold);

    // Local history isn't expired *immediately* once it goes past the expiry
    // threshold. To avoid duplicates at the switch-over point, make sure to
    // start querying only past the oldest local entry.
    if (!output.empty()) {
      state->remote_end_time_for_continuation =
          std::min(state->remote_end_time_for_continuation, OldestTime(output));
    }

    // Note: QueryHistoryInternal() checks whether a remote request is actually
    // possible and necessary, and returns immediately if not.
    QueryHistoryInternal(std::move(state));
    return;
  }

  if (!web_history_timer_->IsRunning()) {
    ReturnResultsToDriver(std::move(state));
  }
}

void BrowsingHistoryService::OnGetAllAppIds(GetAllAppIdsResult result) {
  driver_->OnGetAllAppIds(result.app_ids);
}

void BrowsingHistoryService::ReturnResultsToDriver(
    scoped_refptr<QueryHistoryState> state) {
  std::vector<HistoryEntry> results;
  bool has_remote_results = !state->remote_results.empty();
  bool group_visits = false;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  group_visits =
      base::FeatureList::IsEnabled(kBrowsingHistorySimilarVisitsGrouping);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

  if (group_visits) {
    results = GroupSimilarVisits(state.get());
  } else {
    // Always merge remote results, because Web History does not deduplicate.
    // Local history should be using per-query deduplication, but if we are in a
    // continuation, it's possible that we have carried over pending entries
    // along with new results, and these two sets may contain duplicates.
    // Assuming every call to Web History is successful, we shouldn't be able
    // to have empty sync results at the same time as we have pending local.
    if (has_remote_results) {
      MergeDuplicateResults(state.get(), &results);
    } else {
      // TODO(skym): Is the optimization to skip merge on local only results
      // worth the complexity increase here?
      if (state->local_status == MORE_RESULTS &&
          !state->local_results.empty()) {
        state->local_end_time_for_continuation =
            state->local_results.rbegin()->time;
      }
      results = std::move(state->local_results);
      state->local_results.clear();
    }
  }

  RecordResultsMetrics(results, has_remote_results);

  QueryResultsInfo info;
  info.search_text = state->search_text;
  info.reached_beginning =
      !CanRetry(state->local_status) && !CanRetry(state->remote_status);
  info.sync_timed_out = state->remote_status == TIMED_OUT;
  base::OnceClosure continuation =
      base::BindOnce(&BrowsingHistoryService::QueryHistoryInternal,
                     weak_factory_.GetWeakPtr(), std::move(state));
  driver_->OnQueryComplete(results, info, std::move(continuation));
  driver_->HasOtherFormsOfBrowsingHistory(has_other_forms_of_browsing_history_,
                                          has_synced_results_);
}

void BrowsingHistoryService::RecordResultsMetrics(
    const std::vector<HistoryEntry>& results,
    bool has_remote_results) {
  // Count the number of local, remote, and combined entries, each split by
  // entries before vs after the local expiry threshold (90 days).
  const base::Time local_expiry_threshold =
      clock_->Now() - base::Days(HistoryBackend::kExpireDaysThreshold);
  base::flat_map<HistoryEntry::EntryType, size_t> pre_expiry_counts;
  base::flat_map<HistoryEntry::EntryType, size_t> post_expiry_counts;
  for (const HistoryEntry& entry : results) {
    if (entry.time < local_expiry_threshold) {
      ++pre_expiry_counts[entry.entry_type];
    } else {
      ++post_expiry_counts[entry.entry_type];
    }
  }

  // Note: The histogram max of 150 is chosen to match `RESULTS_PER_PAGE` from
  // chrome/browser/resources/history/constants.ts and `kMaxQueryCount` from
  // chrome/browser/android/history/browsing_history_bridge.cc.
  base::UmaHistogramCustomCounts(
      "History.BrowsingHistoryResult.LocalOnly.PreExpiryThreshold",
      pre_expiry_counts[HistoryEntry::LOCAL_ENTRY], 0, 150, 50);
  base::UmaHistogramCustomCounts(
      "History.BrowsingHistoryResult.LocalOnly.PostExpiryThreshold",
      post_expiry_counts[HistoryEntry::LOCAL_ENTRY], 0, 150, 50);
  base::UmaHistogramCustomCounts(
      "History.BrowsingHistoryResult.RemoteOnly.PreExpiryThreshold",
      pre_expiry_counts[HistoryEntry::REMOTE_ENTRY], 0, 150, 50);
  base::UmaHistogramCustomCounts(
      "History.BrowsingHistoryResult.RemoteOnly.PostExpiryThreshold",
      post_expiry_counts[HistoryEntry::REMOTE_ENTRY], 0, 150, 50);
  base::UmaHistogramCustomCounts(
      "History.BrowsingHistoryResult.Combined.PreExpiryThreshold",
      pre_expiry_counts[HistoryEntry::COMBINED_ENTRY], 0, 150, 50);
  base::UmaHistogramCustomCounts(
      "History.BrowsingHistoryResult.Combined.PostExpiryThreshold",
      post_expiry_counts[HistoryEntry::COMBINED_ENTRY], 0, 150, 50);

  // The "WebHistoryMergeResult" histograms are only recorded if there were any
  // remote results, i.e. an actual merge happened.
  // TODO(crbug.com/456079210): Clean up these histograms once the
  // "History.BrowsingHistoryResult.*" ones are established.
  if (has_remote_results) {
    // Note: The histogram max of 150 is chosen to match `RESULTS_PER_PAGE` from
    // chrome/browser/resources/history/constants.ts and `kMaxQueryCount` from
    // chrome/browser/android/history/browsing_history_bridge.cc.
    base::UmaHistogramCustomCounts(
        "History.WebHistoryMergeResult.LocalOnly.PreExpiryThreshold",
        pre_expiry_counts[HistoryEntry::LOCAL_ENTRY], 0, 150, 50);
    base::UmaHistogramCustomCounts(
        "History.WebHistoryMergeResult.LocalOnly.PostExpiryThreshold",
        post_expiry_counts[HistoryEntry::LOCAL_ENTRY], 0, 150, 50);
    base::UmaHistogramCustomCounts(
        "History.WebHistoryMergeResult.RemoteOnly.PreExpiryThreshold",
        pre_expiry_counts[HistoryEntry::REMOTE_ENTRY], 0, 150, 50);
    base::UmaHistogramCustomCounts(
        "History.WebHistoryMergeResult.RemoteOnly.PostExpiryThreshold",
        post_expiry_counts[HistoryEntry::REMOTE_ENTRY], 0, 150, 50);
    base::UmaHistogramCustomCounts(
        "History.WebHistoryMergeResult.Combined.PreExpiryThreshold",
        pre_expiry_counts[HistoryEntry::COMBINED_ENTRY], 0, 150, 50);
    base::UmaHistogramCustomCounts(
        "History.WebHistoryMergeResult.Combined.PostExpiryThreshold",
        post_expiry_counts[HistoryEntry::COMBINED_ENTRY], 0, 150, 50);
  }

  RecordDuplicateVisitsCount(results);
}

void BrowsingHistoryService::RecordDuplicateVisitsCount(
    const std::vector<HistoryEntry>& results) {
  int duplicate_visits_count = 0;
  for (const HistoryEntry& entry : results) {
    for (const auto& [url, timestamps] : entry.all_timestamps) {
      // Omit the timestamp for the entry itself from the duplicate count.
      url == entry.url ? duplicate_visits_count += timestamps.size() - 1
                        : duplicate_visits_count += timestamps.size();
    }
  }

  // Note: The histogram max of 150 is chosen to match `RESULTS_PER_PAGE` from
  // chrome/browser/resources/history/constants.ts and `kMaxQueryCount` from
  // chrome/browser/android/history/browsing_history_bridge.cc.
  base::UmaHistogramCustomCounts(
      "History.BrowsingHistoryResult.DuplicateVisitsCount",
      duplicate_visits_count, 0, 150, 50);
}

void BrowsingHistoryService::WebHistoryQueryComplete(
    scoped_refptr<QueryHistoryState> state,
    base::Time start_time,
    WebHistoryService::Request* request,
    base::optional_ref<const WebHistoryService::QueryHistoryResult>
        query_history_result) {
  // If the response came in too late, do nothing.
  // TODO(dubroy): Maybe show a banner, and prompt the user to reload?
  if (!web_history_timer_->IsRunning()) {
    return;
  }
  web_history_timer_->Stop();
  web_history_request_.reset();

  if (query_history_result.has_value()) {
    has_synced_results_ = true;

    state->remote_results.reserve(state->remote_results.size() +
                                  query_history_result->visits.size());
    std::string host_name_utf8 = base::UTF16ToUTF8(state->search_text);
    for (const WebHistoryService::QueryHistoryResult::Visit& visit :
         query_history_result->visits) {
      if (state->original_options.host_only) {
        // Do post filtering to skip entries that do not have the correct
        // hostname.
        if (visit.url.GetHost() != host_name_utf8) {
          continue;
        }
      }

      // Ignore any URLs that should not be shown in the history page.
      if (driver_->ShouldHideWebHistoryUrl(visit.url)) {
        continue;
      }

      state->remote_results.emplace_back(
          HistoryEntry::REMOTE_ENTRY, visit.url, base::UTF8ToUTF16(visit.title),
          visit.timestamp, visit.client_id, !state->search_text.empty(),
          std::u16string(),
          /*blocked_visit=*/false, visit.favicon_url, 0, 0,
          /*is_actor_visit=*/false,
          /*app_id=*/std::nullopt);
    }
    state->remote_status = query_history_result->has_more_results
                               ? MORE_RESULTS
                               : REACHED_BEGINNING;
  } else {
    has_synced_results_ = false;
    state->remote_status = FAILURE;
  }

  if (!query_task_tracker_.HasTrackedTasks()) {
    ReturnResultsToDriver(std::move(state));
  }
}

void BrowsingHistoryService::OtherFormsOfBrowsingHistoryQueryComplete(
    bool found_other_forms_of_browsing_history) {
  has_other_forms_of_browsing_history_ = found_other_forms_of_browsing_history;
  driver_->HasOtherFormsOfBrowsingHistory(has_other_forms_of_browsing_history_,
                                          has_synced_results_);
}

void BrowsingHistoryService::RemoveComplete() {
  // Notify the driver that the deletion request is complete, but only if
  // web history delete request is not still pending.
  if (!has_pending_delete_request_) {
    driver_->OnRemoveVisitsComplete();
  }
}

void BrowsingHistoryService::RemoveWebHistoryComplete(bool success) {
  has_pending_delete_request_ = false;
  // TODO(dubroy): Should we handle failure somehow? Delete directives will
  // ensure that the visits are eventually deleted, so maybe it's not necessary.
  if (!delete_task_tracker_.HasTrackedTasks()) {
    RemoveComplete();
  }
}

void BrowsingHistoryService::OnHistoryDeletions(
    HistoryService* history_service,
    const DeletionInfo& deletion_info) {
  // TODO(calamity): Only ignore history deletions when they are actually
  // initiated by us, rather than ignoring them whenever we are deleting.
  if (!delete_task_tracker_.HasTrackedTasks()) {
    driver_->HistoryDeleted();
  }
}

void BrowsingHistoryService::OnWebHistoryDeleted() {
  // TODO(calamity): Only ignore web history deletions when they are actually
  // initiated by us, rather than ignoring them whenever we are deleting.
  if (!has_pending_delete_request_) {
    driver_->HistoryDeleted();
  }
}

}  // namespace history
