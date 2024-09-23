// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_LOG_STORE_H_
#define COMPONENTS_METRICS_METRICS_LOG_STORE_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/metrics/histogram_base.h"
#include "base/sequence_checker.h"
#include "components/metrics/log_store.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_logs_event_manager.h"
#include "components/metrics/unsent_log_store.h"

class PrefService;
class PrefRegistrySimple;

namespace metrics {

class MetricsServiceClient;

// A LogStore implementation for storing UMA logs.
// This implementation keeps track of two types of logs, initial and ongoing,
// each stored in UnsentLogStore. It prioritizes staging initial logs over
// ongoing logs.
//
// An alternate log store can be set to persist ongoing logs. For example, this
// can be used to separate user logs from device logs on Chrome OS. If set, all
// ongoing logs will be written to this alternate log store. Ongoing logs from
// the alternate log store will be prioritized over ongoing logs from the native
// ongoing log store when logs are staged. If an alternate log store is bound,
// then logs will be prioritized in the following order: initial, alternate
// ongoing, native ongoing.
class MetricsLogStore : public LogStore {
 public:
  // Configurable limits for ensuring and restricting local log storage.
  struct StorageLimits {
    // Log store limits for |initial_log_queue_|. See
    // comments at //components/metrics/unsent_log_store.h for more details.
    UnsentLogStore::UnsentLogStoreLimits initial_log_queue_limits;

    // Log store limits for |ongoing_log_queue_|.See
    // comments at //components/metrics/unsent_log_store.h for more details.
    UnsentLogStore::UnsentLogStoreLimits ongoing_log_queue_limits;
  };

  // Constructs a MetricsLogStore that persists data into |local_state|.
  // |storage_limits| provides log count and size limits to enforce when
  // persisting logs to local storage. |signing_key| is used to generate a
  // signature of a log, which will be uploaded to validate data integrity.
  // |logs_event_manager| is used to notify observers of log events. Can be set
  // to null if observing the events is not necessary.
  MetricsLogStore(PrefService* local_state,
                  StorageLimits storage_limits,
                  const std::string& signing_key,
                  MetricsLogsEventManager* logs_event_manager);

  MetricsLogStore(const MetricsLogStore&) = delete;
  MetricsLogStore& operator=(const MetricsLogStore&) = delete;

  ~MetricsLogStore() override;

  // Registers local state prefs used by this class.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Saves |log_data| as the given |log_type|. Before being stored, the data
  // will be compressed, and a hash and signature will be computed.
  // TODO(crbug.com/40119012): Remove this function, and use StoreLogInfo()
  // everywhere instead.
  void StoreLog(const std::string& log_data,
                MetricsLog::LogType log_type,
                const LogMetadata& log_metadata,
                MetricsLogsEventManager::CreateReason reason);

  // Saves a log, represented by a LogInfo object, as the given |log_type|. This
  // is useful if the LogInfo instance needs to be created outside the main
  // thread (since creating a LogInfo from log data requires heavy work). Note
  // that we also pass the size of the log data before being compressed. This
  // is simply for calculating and emitting some metrics, and is otherwise
  // unused.
  void StoreLogInfo(std::unique_ptr<UnsentLogStore::LogInfo> log_info,
                    size_t uncompressed_log_size,
                    MetricsLog::LogType log_type,
                    MetricsLogsEventManager::CreateReason reason);

  // Deletes all logs, in memory and on disk.
  void Purge();

  // Returns the signing key that should be used to create a signature for a
  // log of the given |log_type|. We don't "simply" return the signing key that
  // was passed during the construction of this object, because although
  // |initial_log_queue_| and |ongoing_log_queue_| are also created with the
  // that same signing key, |alternate_ongoing_log_queue_| is provided
  // externally (see |SetAlternateOngoingLogStore()|), which means it could
  // theoretically be created with a different signing key (although unlikely).
  const std::string& GetSigningKeyForLogType(MetricsLog::LogType log_type);

  // Binds an alternate log store to be managed by |this|. All ongoing logs
  // after this call will be written to |log_store| until it is unset. Only one
  // alternate log store can be bound at a time. Returns true if log store is
  // bound successfully.
  //
  // If an alternate log store is already bound, this function will not bind
  // |log_store| and return false.
  //
  // This should be called after |LoadPersistedUnsentLogs()| and after
  // initialization.
  void SetAlternateOngoingLogStore(std::unique_ptr<UnsentLogStore> log_store);

  // Unsets the alternate log store by flushing all existing logs to persistent
  // storage before destructing the alternate log store.
  //
  // If no alternate log store is bound, then this function no-ops.
  void UnsetAlternateOngoingLogStore();

  // LogStore:
  bool has_unsent_logs() const override;
  bool has_staged_log() const override;
  const std::string& staged_log() const override;
  const std::string& staged_log_hash() const override;
  const std::string& staged_log_signature() const override;
  std::optional<uint64_t> staged_log_user_id() const override;
  const LogMetadata staged_log_metadata() const override;
  void StageNextLog() override;
  void DiscardStagedLog(std::string_view reason = "") override;
  void MarkStagedLogAsSent() override;
  void TrimAndPersistUnsentLogs(bool overwrite_in_memory_store) override;
  void LoadPersistedUnsentLogs() override;

  // Inspection methods for tests.
  size_t ongoing_log_count() const { return ongoing_log_queue_.size(); }
  size_t initial_log_count() const { return initial_log_queue_.size(); }

  // Returns true if alternate log store is set.
  bool has_alternate_ongoing_log_store() const;

 private:
  // Returns the log queue of the staged log.
  const UnsentLogStore* get_staged_log_queue() const;

  // Returns true if alternate log store is set and it has unsent logs.
  bool alternate_ongoing_log_store_has_unsent_logs() const;

  // Returns true if alternate log store is set and it has a staged log.
  bool alternate_ongoing_log_store_has_staged_log() const;

  // Returns the log store for given a |log_type|.
  UnsentLogStore* GetLogStoreForLogType(MetricsLog::LogType log_type);

  // Tracks whether unsent logs (if any) have been loaded from the serializer.
  bool unsent_logs_loaded_;

  // Event manager to notify observers of log events.
  const raw_ptr<MetricsLogsEventManager> logs_event_manager_;

  // Logs stored with the INITIAL_STABILITY_LOG type that haven't been sent yet.
  // These logs will be staged first when staging new logs.
  UnsentLogStore initial_log_queue_;
  // Logs stored with the ONGOING_LOG type that haven't been sent yet.
  UnsentLogStore ongoing_log_queue_;
  // Alternate place to store logs stored with ONGOING_LOG type that haven't
  // been sent yet. If initialized, all logs of type ONGOING_LOG will be stored
  // here instead of |ongoing_log_queue_|.
  std::unique_ptr<UnsentLogStore> alternate_ongoing_log_queue_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_LOG_STORE_H_
