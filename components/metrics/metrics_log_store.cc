// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_log_store.h"

#include <string_view>

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
      initial_log_queue_(
          std::make_unique<UnsentLogStoreMetricsImpl>(),
          local_state,
          prefs::kMetricsInitialLogs,
          prefs::kMetricsInitialLogsMetadata,
          UnsentLogStore::UnsentLogStoreLimits{
              storage_limits.initial_log_queue_limits.min_log_count,
              storage_limits.initial_log_queue_limits.min_queue_size_bytes,
              // Each individual initial log can be any size.
              /*max_log_size_bytes=*/0},
          signing_key,
          logs_event_manager),
      ongoing_log_queue_(std::make_unique<UnsentLogStoreMetricsImpl>(),
                         local_state,
                         prefs::kMetricsOngoingLogs,
                         prefs::kMetricsOngoingLogsMetadata,
                         storage_limits.ongoing_log_queue_limits,
                         signing_key,
                         logs_event_manager) {}

MetricsLogStore::~MetricsLogStore() = default;

void MetricsLogStore::LoadPersistedUnsentLogs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  {
    MetricsLogsEventManager::ScopedNotifyLogType scoped_log_type(
        logs_event_manager_, MetricsLog::LogType::INITIAL_STABILITY_LOG);
    initial_log_queue_.LoadPersistedUnsentLogs();
  }

  {
    // Note that we assume that logs loaded from the persistent storage for
    // |ongoing_log_queue_| are of type "ongoing". They could, however, be
    // independent logs, but we unfortunately cannot determine this since we
    // don't persist the type of log.
    MetricsLogsEventManager::ScopedNotifyLogType scoped_log_type(
        logs_event_manager_, MetricsLog::LogType::ONGOING_LOG);
    ongoing_log_queue_.LoadPersistedUnsentLogs();
  }

  unsent_logs_loaded_ = true;
}

void MetricsLogStore::StoreLog(const std::string& log_data,
                               MetricsLog::LogType log_type,
                               const LogMetadata& log_metadata,
                               MetricsLogsEventManager::CreateReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  MetricsLogsEventManager::ScopedNotifyLogType scoped_log_type(
      logs_event_manager_, log_type);
  GetLogStoreForLogType(log_type)->StoreLog(log_data, log_metadata, reason);
}

void MetricsLogStore::StoreLogInfo(
    std::unique_ptr<UnsentLogStore::LogInfo> log_info,
    size_t uncompressed_log_size,
    MetricsLog::LogType log_type,
    MetricsLogsEventManager::CreateReason reason) {
  MetricsLogsEventManager::ScopedNotifyLogType scoped_log_type(
      logs_event_manager_, log_type);
  GetLogStoreForLogType(log_type)->StoreLogInfo(std::move(log_info),
                                                uncompressed_log_size, reason);
}

void MetricsLogStore::Purge() {
  initial_log_queue_.Purge();
  ongoing_log_queue_.Purge();
  if (has_alternate_ongoing_log_store()) {
    alternate_ongoing_log_queue_->Purge();
  }
}

const std::string& MetricsLogStore::GetSigningKeyForLogType(
    MetricsLog::LogType log_type) {
  return GetLogStoreForLogType(log_type)->signing_key();
}

void MetricsLogStore::SetAlternateOngoingLogStore(
    std::unique_ptr<UnsentLogStore> log_store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(!has_alternate_ongoing_log_store());
  DCHECK(unsent_logs_loaded_);
  alternate_ongoing_log_queue_ = std::move(log_store);
  alternate_ongoing_log_queue_->SetLogsEventManager(logs_event_manager_);

  // Note that we assume that logs loaded from the persistent storage for
  // |alternate_ongoing_log_queue_| are of type "ongoing". They could, however,
  // be independent logs, but we unfortunately cannot determine this since we
  // don't persist the type of log.
  MetricsLogsEventManager::ScopedNotifyLogType scoped_log_type(
      logs_event_manager_, MetricsLog::LogType::ONGOING_LOG);
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

std::optional<uint64_t> MetricsLogStore::staged_log_user_id() const {
  return get_staged_log_queue()->staged_log_user_id();
}

const LogMetadata MetricsLogStore::staged_log_metadata() const {
  return get_staged_log_queue()->staged_log_metadata();
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

UnsentLogStore* MetricsLogStore::GetLogStoreForLogType(
    MetricsLog::LogType log_type) {
  switch (log_type) {
    case MetricsLog::INITIAL_STABILITY_LOG:
      return &initial_log_queue_;
    case MetricsLog::ONGOING_LOG:
    case MetricsLog::INDEPENDENT_LOG:
      return has_alternate_ongoing_log_store()
                 ? alternate_ongoing_log_queue_.get()
                 : &ongoing_log_queue_;
  }
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

void MetricsLogStore::DiscardStagedLog(std::string_view reason) {
  DCHECK(has_staged_log());
  if (initial_log_queue_.has_staged_log())
    initial_log_queue_.DiscardStagedLog(reason);
  else if (alternate_ongoing_log_store_has_staged_log())
    alternate_ongoing_log_queue_->DiscardStagedLog(reason);
  else if (ongoing_log_queue_.has_staged_log())
    ongoing_log_queue_.DiscardStagedLog(reason);

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
