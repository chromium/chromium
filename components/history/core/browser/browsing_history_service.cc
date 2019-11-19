// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/browsing_history_service.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/history/core/browser/browsing_history_driver.h"
#include "components/history/core/browser/history_types.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/sync/protocol/history_delete_directive_specifics.pb.h"

namespace history {

namespace {

// The amount of time to wait for a response from the WebHistoryService.
constexpr int kWebHistoryTimeoutSeconds = 3;

// Buckets for UMA histograms.
enum WebHistoryQueryBuckets {
  WEB_HISTORY_QUERY_FAILED = 0,
  WEB_HISTORY_QUERY_SUCCEEDED,
  WEB_HISTORY_QUERY_TIMED_OUT,
  NUM_WEB_HISTORY_QUERY_BUCKETS
};

void RecordMetricsForNoticeAboutOtherFormsOfBrowsingHistory(bool shown) {
  UMA_HISTOGRAM_BOOLEAN("History.ShownHeaderAboutOtherFormsOfBrowsingHistory",
                        shown);
}

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
  return status == MORE_RESULTS || status == FAILURE || status == TIMED_OUT;
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

  base::string16 search_text;
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
    const base::string16& title,
    base::Time time,
    const std::string& client_id,
    bool is_search_result,
    const base::string16& snippet,
    bool blocked_visit,
    const GURL& remote_icon_url_for_uma)
    : entry_type(entry_type),
      url(url),
      title(title),
      time(time),
      client_id(client_id),
      is_search_result(is_search_result),
      snippet(snippet),
      blocked_visit(blocked_visit),
      remote_icon_url_for_uma(remote_icon_url_for_uma) {
  all_timestamps.insert(time.ToInternalValue());
}

BrowsingHistoryService::HistoryEntry::HistoryEntry()
    : entry_type(EMPTY_ENTRY), is_search_result(false), blocked_visit(false) {}

BrowsingHistoryService::HistoryEntry::HistoryEntry(const HistoryEntry& other) =
    default;

BrowsingHistoryService::HistoryEntry::~HistoryEntry() {}

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
  if (local_history_)
    history_service_observer_.Add(local_history_);

  // Get notifications when web history is deleted.
  WebHistoryService* web_history = driver_->GetWebHistoryService();
  if (web_history) {
    web_history_service_observer_.Add(web_history);
  } else if (sync_service_) {
    // If |web_history| is not available, it means that history sync is
    // disabled. If |sync_service_| is not null, it means that syncing is
    // possible, and that history sync/web history may become enabled later, so
    // attach start observing. If |sync_service_| is null then we cannot start
    // observing. This is okay because sync will never start for us, for example
    // it may be disabled by flag or we're part of an incognito/guest mode
    // window.
    sync_service_observer_.Add(sync_service_);
  }
}

BrowsingHistoryService::~BrowsingHistoryService() {
  query_task_tracker_.TryCancelAll();
  web_history_request_.reset();
}

void BrowsingHistoryService::OnStateChanged(syncer::SyncService* sync) {
  // If the history sync was enabled, start observing WebHistoryService.
  // This method should not be called after we already added the observer.
  WebHistoryService* web_history = driver_->GetWebHistoryService();
  if (web_history) {
    DCHECK(!web_history_service_observer_.IsObserving(web_history));
    web_history_service_observer_.Add(web_history);
    sync_service_observer_.RemoveAll();
  }
}

void BrowsingHistoryService::WebHistoryTimeout(
    scoped_refptr<QueryHistoryState> state) {
  state->remote_status = TIMED_OUT;

  // Don't reset |web_history_request_| so we can still record histogram.
  // TODO(dubroy): Communicate the failure to the front end.
  if (!query_task_tracker_.HasTrackedTasks())
    ReturnResultsToDriver(std::move(state));

  UMA_HISTOGRAM_ENUMERATION("WebHistory.QueryCompletion",
                            WEB_HISTORY_QUERY_TIMED_OUT,
                            NUM_WEB_HISTORY_QUERY_BUCKETS);
}

void BrowsingHistoryService::QueryHistory(const base::string16& search_text,
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
  size_t desired_count =
      static_cast<size_t>(state->original_options.EffectiveMaxCount());

  if (local_history_) {
    if (state->local_results.size() < desired_count &&
        state->local_status != REACHED_BEGINNING) {
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
    if (state->remote_results.size() < desired_count &&
        state->remote_status != REACHED_BEGINNING) {
      // Start a timer with timeout before we make the actual query, otherwise
      // tests get confused when completion callback is run synchronously.
      web_history_timer_->Start(
          FROM_HERE, base::TimeDelta::FromSeconds(kWebHistoryTimeoutSeconds),
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
      web_history_request_ = web_history->QueryHistory(
          state->search_text,
          OptionsWithEndTime(state->original_options,
                             state->remote_end_time_for_continuation),
          base::Bind(&BrowsingHistoryService::WebHistoryQueryComplete,
                     weak_factory_.GetWeakPtr(), state, clock_->Now()),
          partial_traffic_annotation);

      // Test the existence of other forms of browsing history.
      driver_->ShouldShowNoticeAboutOtherFormsOfBrowsingHistory(
          sync_service_, web_history,
          base::Bind(
              &BrowsingHistoryService::OtherFormsOfBrowsingHistoryQueryComplete,
              weak_factory_.GetWeakPtr()));
    }
  } else {
    state->remote_status = NO_DEPENDENCY;
    // The notice could not have been shown, because there is no web history.
    RecordMetricsForNoticeAboutOtherFormsOfBrowsingHistory(false);
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

  DCHECK(urls_to_be_deleted_.empty());
  for (const BrowsingHistoryService::HistoryEntry& entry : items) {
    // In order to ensure that visits will be deleted from the server and other
    // clients (even if they are offline), create a sync delete directive for
    // each visit to be deleted.
    sync_pb::HistoryDeleteDirectiveSpecifics delete_directive;
    sync_pb::GlobalIdDirective* global_id_directive =
        delete_directive.mutable_global_id_directive();
    ExpireHistoryArgs* expire_args = nullptr;

    for (int64_t timestamp : entry.all_timestamps) {
      if (!expire_args) {
        GURL gurl(entry.url);
        expire_list.resize(expire_list.size() + 1);
        expire_args = &expire_list.back();
        expire_args->SetTimeRangeForOneDay(
            base::Time::FromInternalValue(timestamp));
        expire_args->urls.insert(gurl);
        urls_to_be_deleted_.insert(gurl);
      }
      // The local visit time is treated as a global ID for the visit.
      global_id_directive->add_global_id(timestamp);
    }

    // Set the start and end time in microseconds since the Unix epoch.
    global_id_directive->set_start_time_usec(
        (expire_args->begin_time - base::Time::UnixEpoch()).InMicroseconds());

    // Delete directives shouldn't have an end time in the future.
    // TODO(dubroy): Use sane time (crbug.com/146090) here when it's ready.
    base::Time end_time = std::min(expire_args->end_time, now);

    // -1 because end time in delete directives is inclusive.
    global_id_directive->set_end_time_usec(
        (end_time - base::Time::UnixEpoch()).InMicroseconds() - 1);

    // TODO(dubroy): Figure out the proper way to handle an error here.
    if (web_history && local_history_)
      local_history_->ProcessLocalDeleteDirective(delete_directive);
  }

  if (local_history_) {
    local_history_->ExpireHistory(
        expire_list,
        base::Bind(&BrowsingHistoryService::RemoveComplete,
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
        base::Bind(&BrowsingHistoryService::RemoveWebHistoryComplete,
                   weak_factory_.GetWeakPtr()),
        partial_traffic_annotation);
  }

  driver_->OnRemoveVisits(expire_list);
}

// static
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
  // later on not doing this could lead to the vector being resized and to
  // pointers to invalid locations.
  std::vector<HistoryEntry> deduped;
  deduped.reserve(sorted.size());

  // Maps a URL to the most recent entry on a particular day.
  std::map<GURL, HistoryEntry*> current_day_entries;

  // Keeps track of the day that |current_day_urls| is holding the URLs for,
  // in order to handle removing per-day duplicates.
  base::Time current_day_midnight;

  for (HistoryEntry& entry : sorted) {
    // Reset the list of found URLs when a visit from a new day is encountered.
    if (current_day_midnight != entry.time.LocalMidnight()) {
      current_day_entries.clear();
      current_day_midnight = entry.time.LocalMidnight();
    }

    // Keep this visit if it's the first visit to this URL on the current day.
    if (current_day_entries.count(entry.url) == 0) {
      const auto entry_url = entry.url;
      deduped.push_back(std::move(entry));
      current_day_entries[entry_url] = &deduped.back();
    } else {
      // Keep track of the timestamps of all visits to the URL on the same day.
      HistoryEntry* matching_entry = current_day_entries[entry.url];
      matching_entry->all_timestamps.insert(entry.all_timestamps.begin(),
                                            entry.all_timestamps.end());

      if (matching_entry->entry_type != entry.entry_type) {
        matching_entry->entry_type = HistoryEntry::COMBINED_ENTRY;
      }

      // Get first non-empty remote icon url.
      if (matching_entry->remote_icon_url_for_uma.is_empty() &&
          !entry.remote_icon_url_for_uma.is_empty()) {
        matching_entry->remote_icon_url_for_uma = entry.remote_icon_url_for_uma;
      }
    }
  }

  // If the beginning of either source was not reached, that means there are
  // more results from that source, and then other source needs to have its data
  // held back until the former source catches up. This only send the UI history
  // entries in the correct order. Subsequent continuation requests will get the
  // delayed entries.
  base::Time oldest_allowed = base::Time();
  if (state->local_status == MORE_RESULTS) {
    oldest_allowed = std::max(oldest_allowed, oldest_local);
    state->local_end_time_for_continuation = oldest_local;
  }
  if (state->remote_status == MORE_RESULTS) {
    oldest_allowed = std::max(oldest_allowed, oldest_remote);
    state->remote_end_time_for_continuation = oldest_remote;
  } else if (CanRetry(state->remote_status)) {
    // TODO(skym): It is unclear if this is the best behavior. The UI is going
    // to behave incorrectly if out of order results are received. So to
    // guarantee that doesn't happen, use |oldest_local| for continuation
    // calls. This will result in missing history entries for the failed calls.
    // crbug.com/685866 is related to this problem.
    state->remote_end_time_for_continuation = oldest_local;
  }

  HistoryEntry search_entry;
  search_entry.time = oldest_allowed;
  auto threshold_iter =
      std::upper_bound(deduped.begin(), deduped.end(), search_entry,
                       HistoryEntry::SortByTimeDescending);

  // Everything from threshold_iter to deduped.end() should either be all local
  // or all remote, never a mix.
  if (threshold_iter != deduped.end()) {
    if (threshold_iter->entry_type == HistoryEntry::LOCAL_ENTRY) {
      state->local_results.assign(std::make_move_iterator(threshold_iter),
                                  std::make_move_iterator(deduped.end()));
    } else if (threshold_iter->entry_type == HistoryEntry::REMOTE_ENTRY) {
      state->remote_results.assign(std::make_move_iterator(threshold_iter),
                                   std::make_move_iterator(deduped.end()));
    } else {
      NOTREACHED();
    }
    deduped.erase(threshold_iter, deduped.end());
  }
  *results = std::move(deduped);
}

void BrowsingHistoryService::QueryComplete(
    scoped_refptr<QueryHistoryState> state,
    QueryResults results) {
  std::vector<HistoryEntry>& output = state->local_results;
  output.reserve(output.size() + results.size());

  for (const auto& page : results) {
    // TODO(dubroy): Use sane time (crbug.com/146090) here when it's ready.
    output.emplace_back(HistoryEntry(
        HistoryEntry::LOCAL_ENTRY, page.url(), page.title(), page.visit_time(),
        std::string(), !state->search_text.empty(), page.snippet().text(),
        page.blocked_visit(), GURL()));
  }

  state->local_status =
      results.reached_beginning() ? REACHED_BEGINNING : MORE_RESULTS;

  if (!web_history_timer_->IsRunning())
    ReturnResultsToDriver(std::move(state));
}

void BrowsingHistoryService::ReturnResultsToDriver(
    scoped_refptr<QueryHistoryState> state) {
  std::vector<HistoryEntry> results;

  // Always merge remote results, because Web History does not deduplicate .
  // Local history should be using per-query deduplication, but if we are in a
  // continuation, it's possible that we have carried over pending entries along
  // with new results, and these two sets may contain duplicates. Assuming every
  // call to Web History is successful, we shouldn't be able to have empty sync
  // results at the same time as we have pending local.
  if (!state->remote_results.empty()) {
    MergeDuplicateResults(state.get(), &results);

    // In the best case, we expect that all local results are duplicated on
    // the server. Keep track of how many are missing.
    int combined_count = 0;
    int local_count = 0;
    for (const HistoryEntry& entry : results) {
      if (entry.entry_type == HistoryEntry::LOCAL_ENTRY)
        ++local_count;
      else if (entry.entry_type == HistoryEntry::COMBINED_ENTRY)
        ++combined_count;
    }

    int local_and_combined = combined_count + local_count;
    if (local_and_combined > 0) {
      UMA_HISTOGRAM_PERCENTAGE("WebHistory.LocalResultMissingOnServer",
                               local_count * 100.0 / local_and_combined);
    }

  } else {
    // TODO(skym): Is the optimization to skip merge on local only results worth
    // the complexity increase here?
    if (state->local_status == MORE_RESULTS && !state->local_results.empty()) {
      state->local_end_time_for_continuation =
          state->local_results.rbegin()->time;
    }
    results = std::move(state->local_results);
  }

  QueryResultsInfo info;
  info.search_text = state->search_text;
  info.reached_beginning =
      !CanRetry(state->local_status) && !CanRetry(state->remote_status);
  info.sync_timed_out = state->remote_status == TIMED_OUT;
  info.has_synced_results = state->remote_status == MORE_RESULTS ||
                            state->remote_status == REACHED_BEGINNING;
  base::OnceClosure continuation =
      base::BindOnce(&BrowsingHistoryService::QueryHistoryInternal,
                     weak_factory_.GetWeakPtr(), std::move(state));
  driver_->OnQueryComplete(results, info, std::move(continuation));
  driver_->HasOtherFormsOfBrowsingHistory(has_other_forms_of_browsing_history_,
                                          has_synced_results_);
}

void BrowsingHistoryService::WebHistoryQueryComplete(
    scoped_refptr<QueryHistoryState> state,
    base::Time start_time,
    WebHistoryService::Request* request,
    const base::DictionaryValue* results_value) {
  base::TimeDelta delta = clock_->Now() - start_time;
  UMA_HISTOGRAM_TIMES("WebHistory.ResponseTime", delta);

  // If the response came in too late, do nothing.
  // TODO(dubroy): Maybe show a banner, and prompt the user to reload?
  if (!web_history_timer_->IsRunning())
    return;
  web_history_timer_->Stop();
  web_history_request_.reset();

  UMA_HISTOGRAM_ENUMERATION(
      "WebHistory.QueryCompletion",
      results_value ? WEB_HISTORY_QUERY_SUCCEEDED : WEB_HISTORY_QUERY_FAILED,
      NUM_WEB_HISTORY_QUERY_BUCKETS);

  if (results_value) {
    has_synced_results_ = true;
    const base::ListValue* events = nullptr;
    if (results_value->GetList("event", &events)) {
      state->remote_results.reserve(state->remote_results.size() +
                                    events->GetSize());
      for (unsigned int i = 0; i < events->GetSize(); ++i) {
        const base::DictionaryValue* event = nullptr;
        const base::DictionaryValue* result = nullptr;
        const base::ListValue* results = nullptr;
        const base::ListValue* ids = nullptr;
        base::string16 url;
        base::string16 title;
        base::string16 favicon_url;

        if (!(events->GetDictionary(i, &event) &&
              event->GetList("result", &results) &&
              results->GetDictionary(0, &result) &&
              result->GetString("url", &url) && result->GetList("id", &ids) &&
              ids->GetSize() > 0)) {
          continue;
        }

        // Ignore any URLs that should not be shown in the history page.
        GURL gurl(url);
        if (driver_->ShouldHideWebHistoryUrl(gurl))
          continue;

        // Title is optional, so the return value is ignored here.
        result->GetString("title", &title);

        result->GetString("favicon_url", &favicon_url);

        // Extract the timestamps of all the visits to this URL.
        // They are referred to as "IDs" by the server.
        for (int j = 0; j < static_cast<int>(ids->GetSize()); ++j) {
          const base::DictionaryValue* id = nullptr;
          std::string timestamp_string;
          int64_t timestamp_usec = 0;

          if (!ids->GetDictionary(j, &id) ||
              !id->GetString("timestamp_usec", &timestamp_string) ||
              !base::StringToInt64(timestamp_string, &timestamp_usec)) {
            NOTREACHED() << "Unable to extract timestamp.";
            continue;
          }
          // The timestamp on the server is a Unix time.
          base::Time time = base::Time::UnixEpoch() +
                            base::TimeDelta::FromMicroseconds(timestamp_usec);

          // Get the ID of the client that this visit came from.
          std::string client_id;
          id->GetString("client_id", &client_id);

          state->remote_results.emplace_back(HistoryEntry(
              HistoryEntry::REMOTE_ENTRY, gurl, title, time, client_id,
              !state->search_text.empty(), base::string16(),
              /* blocked_visit */ false, GURL(favicon_url)));
        }
      }
    }
    std::string continuation_token;
    results_value->GetString("continuation_token", &continuation_token);
    state->remote_status =
        continuation_token.empty() ? REACHED_BEGINNING : MORE_RESULTS;
  } else {
    has_synced_results_ = false;
    state->remote_status = FAILURE;
  }

  if (!query_task_tracker_.HasTrackedTasks())
    ReturnResultsToDriver(std::move(state));
}

void BrowsingHistoryService::OtherFormsOfBrowsingHistoryQueryComplete(
    bool found_other_forms_of_browsing_history) {
  has_other_forms_of_browsing_history_ = found_other_forms_of_browsing_history;

  RecordMetricsForNoticeAboutOtherFormsOfBrowsingHistory(
      has_other_forms_of_browsing_history_);

  driver_->HasOtherFormsOfBrowsingHistory(has_other_forms_of_browsing_history_,
                                          has_synced_results_);
}

void BrowsingHistoryService::RemoveComplete() {
  urls_to_be_deleted_.clear();

  // Notify the driver that the deletion request is complete, but only if
  // web history delete request is not still pending.
  if (!has_pending_delete_request_)
    driver_->OnRemoveVisitsComplete();
}

void BrowsingHistoryService::RemoveWebHistoryComplete(bool success) {
  has_pending_delete_request_ = false;
  // TODO(dubroy): Should we handle failure somehow? Delete directives will
  // ensure that the visits are eventually deleted, so maybe it's not necessary.
  if (!delete_task_tracker_.HasTrackedTasks())
    RemoveComplete();
}

// Helper function for Observe that determines if there are any differences
// between the URLs noticed for deletion and the ones we are expecting.
static bool DeletionsDiffer(const URLRows& deleted_rows,
                            const std::set<GURL>& urls_to_be_deleted) {
  if (deleted_rows.size() != urls_to_be_deleted.size())
    return true;
  for (const auto& i : deleted_rows) {
    if (urls_to_be_deleted.find(i.url()) == urls_to_be_deleted.end())
      return true;
  }
  return false;
}

void BrowsingHistoryService::OnURLsDeleted(HistoryService* history_service,
                                           const DeletionInfo& deletion_info) {
  if (deletion_info.IsAllHistory() ||
      DeletionsDiffer(deletion_info.deleted_rows(), urls_to_be_deleted_))
    driver_->HistoryDeleted();
}

void BrowsingHistoryService::OnWebHistoryDeleted() {
  // TODO(calamity): Only ignore web history deletions when they are actually
  // initiated by us, rather than ignoring them whenever we are deleting.
  if (!has_pending_delete_request_)
    driver_->HistoryDeleted();
}

}  // namespace history
