// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_LOG_STORE_H_
#define COMPONENTS_METRICS_METRICS_LOG_STORE_H_

#include <string>

#include "base/macros.h"
#include "base/metrics/histogram_base.h"
#include "base/optional.h"
#include "components/metrics/log_store.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/unsent_log_store.h"

class PrefService;
class PrefRegistrySimple;

namespace metrics {

class MetricsServiceClient;

// A LogStore implementation for storing UMA logs.
// This implementation keeps track of two types of logs, initial and ongoing,
// each stored in UnsentLogStore. It prioritizes staging initial logs over
// ongoing logs.
class MetricsLogStore : public LogStore {
 public:
  // Configurable limits for ensuring and restricting local log storage.
  //
  // |min_{initial,ongoing}_log_queue_count| are the minimum numbers of unsent
  // logs that UnsentLogStore must persist before deleting old logs.
  //
  // |min_{initial,ongoing}_log_queue_size| are the minimum numbers of bytes in
  // total across all logs within the initial or ongoing log queue that
  // UnsentLogStore must persist before deleting old logs.
  //
  // If both |min_..._log_queue_count| and |min_..._log_queue_size| are 0, then
  // this LogStore won't persist unsent logs to local storage.
  //
  // |max_ongoing_log_size| is the maximum size of any individual ongoing log.
  // When set to 0, no limits are imposed, i.e. individual logs can be any size.
  struct StorageLimits {
    size_t min_initial_log_queue_count = 0;
    size_t min_initial_log_queue_size = 0;
    size_t min_ongoing_log_queue_count = 0;
    size_t min_ongoing_log_queue_size = 0;
    size_t max_ongoing_log_size = 0;
  };

  // Constructs a MetricsLogStore that persists data into |local_state|.
  // |storage_limits| provides log count and size limits to enforce when
  // persisting logs to local storage. |signing_key| is used to generate a
  // signature of a log, which will be uploaded to validate data integrity.
  MetricsLogStore(PrefService* local_state,
                  StorageLimits storage_limits,
                  const std::string& signing_key);
  ~MetricsLogStore();

  // Registers local state prefs used by this class.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Saves |log_data| as the given type.
  void StoreLog(const std::string& log_data,
                MetricsLog::LogType log_type,
                base::Optional<base::HistogramBase::Count> samples_count);

  // LogStore:
  bool has_unsent_logs() const override;
  bool has_staged_log() const override;
  const std::string& staged_log() const override;
  const std::string& staged_log_hash() const override;
  const std::string& staged_log_signature() const override;
  void StageNextLog() override;
  void DiscardStagedLog() override;
  void MarkStagedLogAsSent() override;
  void TrimAndPersistUnsentLogs() override;
  void LoadPersistedUnsentLogs() override;

  // Inspection methods for tests.
  size_t ongoing_log_count() const { return ongoing_log_queue_.size(); }
  size_t initial_log_count() const { return initial_log_queue_.size(); }

 private:
  // Tracks whether unsent logs (if any) have been loaded from the serializer.
  bool unsent_logs_loaded_;

  // Logs stored with the INITIAL_STABILITY_LOG type that haven't been sent yet.
  // These logs will be staged first when staging new logs.
  UnsentLogStore initial_log_queue_;
  // Logs stored with the ONGOING_LOG type that haven't been sent yet.
  UnsentLogStore ongoing_log_queue_;

  DISALLOW_COPY_AND_ASSIGN(MetricsLogStore);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_LOG_STORE_H_
