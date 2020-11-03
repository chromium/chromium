// Copyright 2017 The Chromium Authors. All rights reserved.
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
                                 const std::string& signing_key)
    : unsent_logs_loaded_(false),
      initial_log_queue_(std::make_unique<UnsentLogStoreMetricsImpl>(),
                         local_state,
                         prefs::kMetricsInitialLogs,
                         prefs::kMetricsInitialLogsMetadata,
                         storage_limits.min_initial_log_queue_count,
                         storage_limits.min_initial_log_queue_size,
                         0,  // Each individual initial log can be any size.
                         signing_key),
      ongoing_log_queue_(std::make_unique<UnsentLogStoreMetricsImpl>(),
                         local_state,
                         prefs::kMetricsOngoingLogs,
                         prefs::kMetricsOngoingLogsMetadata,
                         storage_limits.min_ongoing_log_queue_count,
                         storage_limits.min_ongoing_log_queue_size,
                         storage_limits.max_ongoing_log_size,
                         signing_key) {}

MetricsLogStore::~MetricsLogStore() {}

void MetricsLogStore::LoadPersistedUnsentLogs() {
  initial_log_queue_.LoadPersistedUnsentLogs();
  ongoing_log_queue_.LoadPersistedUnsentLogs();
  unsent_logs_loaded_ = true;
}

void MetricsLogStore::StoreLog(
    const std::string& log_data,
    MetricsLog::LogType log_type,
    base::Optional<base::HistogramBase::Count> samples_count) {
  switch (log_type) {
    case MetricsLog::INITIAL_STABILITY_LOG:
      initial_log_queue_.StoreLog(log_data, samples_count);
      break;
    case MetricsLog::ONGOING_LOG:
    case MetricsLog::INDEPENDENT_LOG:
      ongoing_log_queue_.StoreLog(log_data, samples_count);
      break;
  }
}

bool MetricsLogStore::has_unsent_logs() const {
  return initial_log_queue_.has_unsent_logs() ||
         ongoing_log_queue_.has_unsent_logs();
}

bool MetricsLogStore::has_staged_log() const {
  return initial_log_queue_.has_staged_log() ||
         ongoing_log_queue_.has_staged_log();
}

const std::string& MetricsLogStore::staged_log() const {
  return initial_log_queue_.has_staged_log() ? initial_log_queue_.staged_log()
                                             : ongoing_log_queue_.staged_log();
}

const std::string& MetricsLogStore::staged_log_hash() const {
  return initial_log_queue_.has_staged_log()
             ? initial_log_queue_.staged_log_hash()
             : ongoing_log_queue_.staged_log_hash();
}

const std::string& MetricsLogStore::staged_log_signature() const {
  return initial_log_queue_.has_staged_log()
             ? initial_log_queue_.staged_log_signature()
             : ongoing_log_queue_.staged_log_signature();
}

void MetricsLogStore::StageNextLog() {
  DCHECK(!has_staged_log());
  if (initial_log_queue_.has_unsent_logs())
    initial_log_queue_.StageNextLog();
  else
    ongoing_log_queue_.StageNextLog();
}

void MetricsLogStore::DiscardStagedLog() {
  DCHECK(has_staged_log());
  if (initial_log_queue_.has_staged_log())
    initial_log_queue_.DiscardStagedLog();
  else
    ongoing_log_queue_.DiscardStagedLog();
  DCHECK(!has_staged_log());
}

void MetricsLogStore::MarkStagedLogAsSent() {
  DCHECK(has_staged_log());
  if (initial_log_queue_.has_staged_log())
    initial_log_queue_.MarkStagedLogAsSent();
  else
    ongoing_log_queue_.MarkStagedLogAsSent();
}

void MetricsLogStore::TrimAndPersistUnsentLogs() {
  DCHECK(unsent_logs_loaded_);
  if (!unsent_logs_loaded_)
    return;

  initial_log_queue_.TrimAndPersistUnsentLogs();
  ongoing_log_queue_.TrimAndPersistUnsentLogs();
}

}  // namespace metrics
