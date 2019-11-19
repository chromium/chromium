// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/counters/history_counter.h"

#include <limits.h>
#include <stdint.h>
#include <memory>

#include "base/bind.h"
#include "base/timer/timer.h"
#include "components/browsing_data/core/pref_names.h"

namespace {
static const int64_t kWebHistoryTimeoutSeconds = 10;
}

namespace browsing_data {

HistoryCounter::HistoryCounter(
    history::HistoryService* history_service,
    const GetUpdatedWebHistoryServiceCallback& callback,
    syncer::SyncService* sync_service)
    : history_service_(history_service),
      web_history_service_callback_(callback),
      sync_tracker_(this, sync_service),
      has_synced_visits_(false),
      local_counting_finished_(false),
      web_counting_finished_(false) {
  DCHECK(history_service_);
}

HistoryCounter::~HistoryCounter() {}

void HistoryCounter::OnInitialized() {
  sync_tracker_.OnInitialized(base::Bind(&HistoryCounter::IsHistorySyncEnabled,
                                         base::Unretained(this)));
}

bool HistoryCounter::HasTrackedTasks() {
  return cancelable_task_tracker_.HasTrackedTasks();
}

const char* HistoryCounter::GetPrefName() const {
  return GetTab() == ClearBrowsingDataTab::BASIC
             ? browsing_data::prefs::kDeleteBrowsingHistoryBasic
             : browsing_data::prefs::kDeleteBrowsingHistory;
}

history::WebHistoryService* HistoryCounter::GetWebHistoryService() {
  if (web_history_service_callback_)
    return web_history_service_callback_.Run();
  return nullptr;
}

void HistoryCounter::Count() {
  // Reset the state.
  cancelable_task_tracker_.TryCancelAll();
  web_history_request_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();

  has_synced_visits_ = false;

  // Count the locally stored items.
  local_counting_finished_ = false;

  history_service_->GetHistoryCount(
      GetPeriodStart(), GetPeriodEnd(),
      base::BindOnce(&HistoryCounter::OnGetLocalHistoryCount,
                     weak_ptr_factory_.GetWeakPtr()),
      &cancelable_task_tracker_);

  // If the history sync is enabled, test if there is at least one synced item.
  history::WebHistoryService* web_history = GetWebHistoryService();

  if (!web_history) {
    web_counting_finished_ = true;
    return;
  }

  web_counting_finished_ = false;

  web_history_timeout_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kWebHistoryTimeoutSeconds), this,
      &HistoryCounter::OnWebHistoryTimeout);

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
  web_history_request_ = web_history->QueryHistory(
      base::string16(), options,
      base::Bind(&HistoryCounter::OnGetWebHistoryCount,
                 weak_ptr_factory_.GetWeakPtr()),
      partial_traffic_annotation);

  // TODO(msramek): Include web history count when there is an API for it.
}

void HistoryCounter::OnGetLocalHistoryCount(
    history::HistoryCountResult result) {
  // Ensure that all callbacks are on the same thread, so that we do not need
  // a mutex for |MergeResults|.
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!result.success) {
    return;
  }

  local_result_ = result.count;
  local_counting_finished_ = true;
  MergeResults();
}

void HistoryCounter::OnGetWebHistoryCount(
    history::WebHistoryService::Request* request,
    const base::DictionaryValue* result) {
  // Ensure that all callbacks are on the same thread, so that we do not need
  // a mutex for |MergeResults|.
  DCHECK(thread_checker_.CalledOnValidThread());

  // If the timeout for this request already fired, ignore the result.
  if (!web_history_timeout_.IsRunning())
    return;

  web_history_timeout_.Stop();

  // If the query failed, err on the safe side and inform the user that they
  // may have history items stored in Sync. Otherwise, we expect at least one
  // entry in the "event" list.
  const base::ListValue* events;
  has_synced_visits_ =
      !result || (result->GetList("event", &events) && !events->empty());
  web_counting_finished_ = true;
  MergeResults();
}

void HistoryCounter::OnWebHistoryTimeout() {
  // Ensure that all callbacks are on the same thread, so that we do not need
  // a mutex for |MergeResults|.
  DCHECK(thread_checker_.CalledOnValidThread());

  // If the query timed out, err on the safe side and inform the user that they
  // may have history items stored in Sync.
  web_history_request_.reset();
  has_synced_visits_ = true;
  web_counting_finished_ = true;
  MergeResults();
}

void HistoryCounter::MergeResults() {
  if (!local_counting_finished_ || !web_counting_finished_)
    return;

  ReportResult(std::make_unique<HistoryResult>(
      this, local_result_, sync_tracker_.IsSyncActive(), has_synced_visits_));
}

bool HistoryCounter::IsHistorySyncEnabled(
    const syncer::SyncService* sync_service) {
  return !!GetWebHistoryService();
}

HistoryCounter::HistoryResult::HistoryResult(const HistoryCounter* source,
                                             ResultInt value,
                                             bool is_sync_enabled,
                                             bool has_synced_visits)
    : SyncResult(source, value, is_sync_enabled),
      has_synced_visits_(has_synced_visits) {}

HistoryCounter::HistoryResult::~HistoryResult() {}

}  // namespace browsing_data
