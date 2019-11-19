// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_log_store.h"

#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/unsent_log_store_metrics_impl.h"
#include "components/prefs/pref_registry_simple.h"

namespace metrics {

namespace {

// The number of "initial" logs to save, and hope to send during a future Chrome
// session. Initial logs contain crash stats, and are pretty small.
const size_t kInitialLogsSaveLimit = 20;

// The number of ongoing logs to save persistently, and hope to
// send during a this or future sessions. Note that each log may be pretty
// large, as presumably the related "initial" log wasn't sent (probably nothing
// was, as the user was probably off-line). As a result, the log probably kept
// accumulating while the "initial" log was stalled, and couldn't be sent. As a
// result, we don't want to save too many of these mega-logs.
// A "standard shutdown" will create a small log, including just the data that
// was not yet been transmitted, and that is normal (to have exactly one
// ongoing_log_ at startup).
const size_t kOngoingLogsSaveLimit = 8;

// The number of bytes of logs to save of each type (initial/ongoing).
// This ensures that a reasonable amount of history will be stored even if there
// is a long series of very small logs.
const size_t kStorageByteLimitPerLogType = 300 * 1000;  // ~300kB

}  // namespace

// static
void MetricsLogStore::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kMetricsInitialLogs);
  registry->RegisterListPref(prefs::kMetricsOngoingLogs);
}

MetricsLogStore::MetricsLogStore(PrefService* local_state,
                                 size_t max_ongoing_log_size,
                                 const std::string& signing_key)
    : unsent_logs_loaded_(false),
      initial_log_queue_(std::make_unique<UnsentLogStoreMetricsImpl>(),
                         local_state,
                         prefs::kMetricsInitialLogs,
                         kInitialLogsSaveLimit,
                         kStorageByteLimitPerLogType,
                         0,
                         signing_key),
      ongoing_log_queue_(std::make_unique<UnsentLogStoreMetricsImpl>(),
                         local_state,
                         prefs::kMetricsOngoingLogs,
                         kOngoingLogsSaveLimit,
                         kStorageByteLimitPerLogType,
                         max_ongoing_log_size,
                         signing_key) {}

MetricsLogStore::~MetricsLogStore() {}

void MetricsLogStore::LoadPersistedUnsentLogs() {
  initial_log_queue_.LoadPersistedUnsentLogs();
  ongoing_log_queue_.LoadPersistedUnsentLogs();
  unsent_logs_loaded_ = true;
}

void MetricsLogStore::StoreLog(const std::string& log_data,
                               MetricsLog::LogType log_type) {
  switch (log_type) {
    case MetricsLog::INITIAL_STABILITY_LOG:
      initial_log_queue_.StoreLog(log_data);
      break;
    case MetricsLog::ONGOING_LOG:
    case MetricsLog::INDEPENDENT_LOG:
      ongoing_log_queue_.StoreLog(log_data);
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

void MetricsLogStore::PersistUnsentLogs() const {
  DCHECK(unsent_logs_loaded_);
  if (!unsent_logs_loaded_)
    return;

  initial_log_queue_.PersistUnsentLogs();
  ongoing_log_queue_.PersistUnsentLogs();
}

}  // namespace metrics
