// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/counters/history_counter.h"

#include <limits.h>
#include <stdint.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/timer/timer.h"
#include "components/browsing_data/core/pref_names.h"

namespace {
static const int64_t kWebHistoryTimeoutSeconds = 10;
}

namespace browsing_data {

HistoryCounter::HistoryCounter(history::HistoryService* history_service,
                               history::WebHistoryService* web_history_service,
                               syncer::SyncService* sync_service)
    : history_service_(history_service),
      web_history_service_(web_history_service),
      sync_tracker_(this, sync_service),
      has_synced_visits_(false),
      web_counting_finished_(false),
      domain_fetching_finished_(false) {
  DCHECK(history_service_);
}

HistoryCounter::~HistoryCounter() = default;

void HistoryCounter::OnInitialized() {
  sync_tracker_.OnInitialized(
      base::IgnoreArgs<const syncer::SyncService*>(base::BindRepeating(
          &HistoryCounter::IsHistorySyncEnabled, base::Unretained(this))));
}

bool HistoryCounter::HasTrackedTasksForTesting() {
  return cancelable_task_tracker_.HasTrackedTasks();
}

const char* HistoryCounter::GetPrefName() const {
  return browsing_data::prefs::kDeleteBrowsingHistory;
}

void HistoryCounter::Count() {
  // Reset the state.
  cancelable_task_tracker_.TryCancelAll();
  web_history_request_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
  web_history_timeout_.Stop();

  has_synced_visits_ = false;

  const bool is_history_sync_enabled = IsHistorySyncEnabled();

  web_counting_finished_ = !is_history_sync_enabled;
  domain_fetching_finished_ = false;

  // Count the locally stored items.
  history_service_->GetUniqueDomainsVisited(
      GetPeriodStart(), GetPeriodEnd(),
      history::VisitQuery404sPolicy::kInclude404s,
      base::BindOnce(&HistoryCounter::OnGetUniqueDomains,
                     weak_ptr_factory_.GetWeakPtr()),
      &cancelable_task_tracker_);

  if (!is_history_sync_enabled) {
    return;
  }

  web_history_timeout_.Start(FROM_HERE,
                             base::Seconds(kWebHistoryTimeoutSeconds), this,
                             &HistoryCounter::OnWebHistoryTimeout);

  // If the history sync is enabled, test if there is at least one synced item.
  history::QueryOptions options;
  options.max_count = 1;
  options.begin_time = GetPeriodStart();
  options.end_time = GetPeriodEnd();
  net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation =
      net::DefinePartialNetworkTrafficAnnotation("web_history_counter",
                                                 "web_history_service",
                                                 R"(
        semantics {
          description:
            "If history sync is enabled, this queries history.google.com to "
            "determine if there is any synced history. This information is "
            "displayed in the Clear Browsing Data dialog."
          trigger:
            "Checking the 'Browsing history' option in the Clear Browsing Data "
            "dialog, or enabling history sync while the dialog is open."
          data:
            "A version info token to resolve transaction conflicts, and an "
            "OAuth2 token authenticating the user."
        }
        policy {
          chrome_policy {
            SyncDisabled {
              SyncDisabled: true
            }
          }
        })");

  web_history_request_ = web_history_service_->QueryHistory(
      std::u16string(), options,
      base::BindOnce(&HistoryCounter::OnGetWebHistoryCount,
                     weak_ptr_factory_.GetWeakPtr()),
      partial_traffic_annotation);

  // TODO(msramek): Include web history count when there is an API for it.
}

void HistoryCounter::OnGetWebHistoryCount(
    history::WebHistoryService::Request* request,
    base::optional_ref<const history::WebHistoryService::QueryHistoryResult>
        result) {
  // Ensure that all callbacks are on the same thread, so that we do not need
  // a mutex for `MergeResults`.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If the timeout for this request already fired, ignore the result.
  if (!web_history_timeout_.IsRunning()) {
    return;
  }

  web_history_timeout_.Stop();

  // If the query failed, err on the safe side and inform the user that they
  // may have history items stored in Sync. Otherwise, we expect at least one
  // entry in the "events" list.
  if (!result.has_value()) {
    has_synced_visits_ = true;
  } else {
    has_synced_visits_ = !result->visits.empty();
  }
  web_counting_finished_ = true;
  MergeResults();
}

void HistoryCounter::OnGetUniqueDomains(history::DomainsVisitedResult result) {
  // Ensure that all callbacks are on the same thread, so that we do not need
  // a mutex for `MergeResults`.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  unique_domains_result_ = result.all_visited_domains.size();
  last_visited_domain_ = result.all_visited_domains.empty()
                             ? ""
                             : result.all_visited_domains.front();

  domain_fetching_finished_ = true;
  MergeResults();
}

void HistoryCounter::OnWebHistoryTimeout() {
  // Ensure that all callbacks are on the same thread, so that we do not need
  // a mutex for `MergeResults`.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If the query timed out, err on the safe side and inform the user that they
  // may have history items stored in Sync.
  web_history_request_.reset();
  has_synced_visits_ = true;
  web_counting_finished_ = true;
  MergeResults();
}

void HistoryCounter::MergeResults() {
  if (!web_counting_finished_ || !domain_fetching_finished_) {
    return;
  }

  ReportResult(std::make_unique<HistoryResult>(
      this, unique_domains_result_, sync_tracker_.IsSyncActive(),
      has_synced_visits_, last_visited_domain_));
}

bool HistoryCounter::IsHistorySyncEnabled() const {
  return web_history_service_ && web_history_service_->IsEnabled();
}

HistoryCounter::HistoryResult::HistoryResult(const HistoryCounter* source,
                                             ResultInt value,
                                             bool is_sync_enabled,
                                             bool has_synced_visits,
                                             std::string last_visited_domain)
    : SyncResult(source, value, is_sync_enabled),
      has_synced_visits_(has_synced_visits),
      last_visited_domain_(std::move(last_visited_domain)) {}

HistoryCounter::HistoryResult::~HistoryResult() = default;

}  // namespace browsing_data
