// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_log_store.h"

#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/unsent_log_store_metrics_impl.h"
#include "components/prefs/pref_registry_simple.h"

namespace metrics {

// static
void MetricsLogStore::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kMetricsInitialLogs);
  registry->RegisterListPref(prefs::kMetricsOngoingLogs);
  registry->RegisterDictionaryPref(prefs::kMetricsInitialLogsMetadata);
  registry->RegisterDictionaryPref(prefs::kMetricsOngoingLogsMetadata);
}

MetricsLogStore::MetricsLogStore(PrefService* local_state,
                                 StorageLimits storage_limits,
                                 const std::string& signing_key,
                                 MetricsLogsEventManager* logs_event_manager)
    : unsent_logs_loaded_(false),
      logs_event_manager_(logs_event_manager),
      initial_log_queue_(std::make_unique<UnsentLogStoreMetricsImpl>(),
                         local_state,
                         prefs::kMetricsInitialLogs,
                         prefs::kMetricsInitialLogsMetadata,
                         storage_limits.min_initial_log_queue_count,
                         storage_limits.min_initial_log_queue_size,
                         0,  // Each individual initial log can be any size.
                         signing_key,
                         logs_event_manager),
      ongoing_log_queue_(std::make_unique<UnsentLogStoreMetricsImpl>(),
                         local_state,
                         prefs::kMetricsOngoingLogs,
                         prefs::kMetricsOngoingLogsMetadata,
                         storage_limits.min_ongoing_log_queue_count,
                         storage_limits.min_ongoing_log_queue_size,
                         storage_limits.max_ongoing_log_size,
                         signing_key,
                         logs_event_manager) {}

MetricsLogStore::~MetricsLogStore() {}

void MetricsLogStore::LoadPersistedUnsentLogs() {
  initial_log_queue_.LoadPersistedUnsentLogs();
  ongoing_log_queue_.LoadPersistedUnsentLogs();
  unsent_logs_loaded_ = true;
}

void MetricsLogStore::StoreLog(const std::string& log_data,
                               MetricsLog::LogType log_type,
                               const LogMetadata& log_metadata) {
  switch (log_type) {
    case MetricsLog::INITIAL_STABILITY_LOG:
      initial_log_queue_.StoreLog(log_data, log_metadata);
      break;
    case MetricsLog::ONGOING_LOG:
    case MetricsLog::INDEPENDENT_LOG:
      has_alternate_ongoing_log_store()
          ? alternate_ongoing_log_queue_->StoreLog(log_data, log_metadata)
          : ongoing_log_queue_.StoreLog(log_data, log_metadata);
      break;
  }
}

void MetricsLogStore::SetAlternateOngoingLogStore(
    std::unique_ptr<UnsentLogStore> log_store) {
  DCHECK(!has_alternate_ongoing_log_store());
  DCHECK(unsent_logs_loaded_);
  alternate_ongoing_log_queue_ = std::move(log_store);
  alternate_ongoing_log_queue_->SetLogsEventManager(logs_event_manager_);
  alternate_ongoing_log_queue_->LoadPersistedUnsentLogs();
}

void MetricsLogStore::UnsetAlternateOngoingLogStore() {
  DCHECK(has_alternate_ongoing_log_store());
  alternate_ongoing_log_queue_->TrimAndPersistUnsentLogs(
      /*overwrite_in_memory_store=*/true);
  alternate_ongoing_log_queue_.reset();
}

bool MetricsLogStore::has_unsent_logs() const {
  return initial_log_queue_.has_unsent_logs() ||
         ongoing_log_queue_.has_unsent_logs() ||
         alternate_ongoing_log_store_has_unsent_logs();
}

bool MetricsLogStore::has_staged_log() const {
  return initial_log_queue_.has_staged_log() ||
         ongoing_log_queue_.has_staged_log() ||
         alternate_ongoing_log_store_has_staged_log();
}

const std::string& MetricsLogStore::staged_log() const {
  return get_staged_log_queue()->staged_log();
}

const std::string& MetricsLogStore::staged_log_hash() const {
  return get_staged_log_queue()->staged_log_hash();
}

const std::string& MetricsLogStore::staged_log_signature() const {
  return get_staged_log_queue()->staged_log_signature();
}

absl::optional<uint64_t> MetricsLogStore::staged_log_user_id() const {
  return get_staged_log_queue()->staged_log_user_id();
}

bool MetricsLogStore::has_alternate_ongoing_log_store() const {
  return alternate_ongoing_log_queue_ != nullptr;
}

const UnsentLogStore* MetricsLogStore::get_staged_log_queue() const {
  DCHECK(has_staged_log());

  // This is the order in which logs should be staged. Should be consistent with
  // StageNextLog.
  if (initial_log_queue_.has_staged_log())
    return &initial_log_queue_;
  else if (alternate_ongoing_log_store_has_staged_log())
    return alternate_ongoing_log_queue_.get();
  return &ongoing_log_queue_;
}

bool MetricsLogStore::alternate_ongoing_log_store_has_unsent_logs() const {
  return has_alternate_ongoing_log_store() &&
         alternate_ongoing_log_queue_->has_unsent_logs();
}

bool MetricsLogStore::alternate_ongoing_log_store_has_staged_log() const {
  return has_alternate_ongoing_log_store() &&
         alternate_ongoing_log_queue_->has_staged_log();
}

void MetricsLogStore::StageNextLog() {
  DCHECK(!has_staged_log());
  if (initial_log_queue_.has_unsent_logs())
    initial_log_queue_.StageNextLog();
  else if (alternate_ongoing_log_store_has_unsent_logs())
    alternate_ongoing_log_queue_->StageNextLog();
  else if (ongoing_log_queue_.has_unsent_logs())
    ongoing_log_queue_.StageNextLog();
}

void MetricsLogStore::DiscardStagedLog() {
  DCHECK(has_staged_log());
  if (initial_log_queue_.has_staged_log())
    initial_log_queue_.DiscardStagedLog();
  else if (alternate_ongoing_log_store_has_staged_log())
    alternate_ongoing_log_queue_->DiscardStagedLog();
  else if (ongoing_log_queue_.has_staged_log())
    ongoing_log_queue_.DiscardStagedLog();

  DCHECK(!has_staged_log());
}

void MetricsLogStore::MarkStagedLogAsSent() {
  DCHECK(has_staged_log());
  if (initial_log_queue_.has_staged_log())
    initial_log_queue_.MarkStagedLogAsSent();
  else if (alternate_ongoing_log_store_has_staged_log())
    alternate_ongoing_log_queue_->MarkStagedLogAsSent();
  else if (ongoing_log_queue_.has_staged_log())
    ongoing_log_queue_.MarkStagedLogAsSent();
}

void MetricsLogStore::TrimAndPersistUnsentLogs(bool overwrite_in_memory_store) {
  DCHECK(unsent_logs_loaded_);
  if (!unsent_logs_loaded_)
    return;

  initial_log_queue_.TrimAndPersistUnsentLogs(overwrite_in_memory_store);
  ongoing_log_queue_.TrimAndPersistUnsentLogs(overwrite_in_memory_store);
  if (has_alternate_ongoing_log_store())
    alternate_ongoing_log_queue_->TrimAndPersistUnsentLogs(
        overwrite_in_memory_store);
}

}  // namespace metrics
