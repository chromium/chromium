// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_UNSENT_LOG_STORE_H_
#define COMPONENTS_METRICS_UNSENT_LOG_STORE_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_base.h"
#include "base/values.h"
#include "components/metrics/log_store.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_logs_event_manager.h"

class PrefService;

namespace metrics {

class UnsentLogStoreMetrics;

// Maintains a list of unsent logs that are written and restored from disk.
class UnsentLogStore : public LogStore {
 public:
  // Configurable capacities for unsent log store.
  //
  // When saving logs to disk, stores either the first |min_log_count| logs, or
  // at least |min_queue_size_bytes| bytes of logs. If |this| contains more than
  // |min_log_count| logs AND a total bytes larger than |min_queue_size_bytes|,
  // older logs will be dropped for newer logs.
  //
  // Either |min_queue_size_bytes| or |min_log_count| must be greater than 0.
  //
  // Individual logs greater than |max_log_size_bytes| will not be written to
  // disk. If |max_log_size_bytes| is zero, logs of any size will be written to
  // disk.
  struct UnsentLogStoreLimits {
    // Minimum number of unsent logs persisted before older logs are trimmed.
    //
    // log_count >= |min_log_count| AND total_queue_bytes >=
    // |min_queue_size_bytes| for logs to be dropped. See comments for
    // UnsentLogStoreLimits for more details.
    size_t min_log_count = 0;

    // Minimum bytes that the queue can hold before older logs are trimmed.
    //
    // Number of logs >= |min_log_count| AND total_queue_size >=
    // |min_queue_size_bytes| for logs to be dropped. See comments for
    // UnsentLogStoreLimits for more details.
    size_t min_queue_size_bytes = 0;

    // Logs greater than this size will not be written to disk.
    size_t max_log_size_bytes = 0;
  };

  // Constructs an UnsentLogStore that stores data in |local_state| under the
  // preference |log_data_pref_name|.
  // Calling code is responsible for ensuring that the lifetime of |local_state|
  // is longer than the lifetime of UnsentLogStore.
  //
  // The optional |metadata_pref_name| is the preference that is used to store
  // the unsent logs info while the unset logs are persisted. That info will be
  // recorded as UMA metrics in next browser startup.
  //
  // |signing_key| is used to produce an HMAC-SHA256 signature of the logged
  // data, which will be uploaded with the log and used to validate data
  // integrity.
  //
  // |logs_event_manager| is used to notify observers of log events. Can be set
  // to null if observing the events is not necessary.
  UnsentLogStore(std::unique_ptr<UnsentLogStoreMetrics> metrics,
                 PrefService* local_state,
                 const char* log_data_pref_name,
                 const char* metadata_pref_name,
                 UnsentLogStoreLimits log_store_limits,
                 const std::string& signing_key,
                 MetricsLogsEventManager* logs_event_manager);

  UnsentLogStore(const UnsentLogStore&) = delete;
  UnsentLogStore& operator=(const UnsentLogStore&) = delete;

  ~UnsentLogStore() override;

  struct LogInfo {
    LogInfo();

    LogInfo(const LogInfo&) = delete;
    LogInfo& operator=(const LogInfo&) = delete;

    ~LogInfo();

    // Initializes the members based on uncompressed |log_data|,
    // |log_timestamp|, and |signing_key|. |log_data| is the uncompressed
    // serialized log protobuf. A hash and a signature are computed from
    // |log_data|. The signature is produced using |signing_key|. |log_data|
    // will be compressed and stored in |compressed_log_data|. |log_timestamp|
    // is stored as is. |log_metadata| is any optional metadata that will be
    // attached to the log.
    // TODO(crbug.com/40119012): Make this a ctor instead.
    void Init(const std::string& log_data,
              const std::string& log_timestamp,
              const std::string& signing_key,
              const LogMetadata& log_metadata);

    // Same as above, but the |timestamp| field will be filled with the current
    // time.
    // TODO(crbug.com/40119012): Make this a ctor instead.
    void Init(const std::string& log_data,
              const std::string& signing_key,
              const LogMetadata& log_metadata);

    // Compressed log data - a serialized protobuf that's been gzipped.
    std::string compressed_log_data;

    // The SHA1 hash of the log. Computed in Init and stored to catch errors
    // from memory corruption.
    std::string hash;

    // The HMAC-SHA256 signature of the log, used to validate the log came from
    // Chrome. It's computed in Init and stored, instead of computed on demand,
    // to catch errors from memory corruption.
    std::string signature;

    // The timestamp of when the log was created as a time_t value.
    std::string timestamp;

    // Properties of the log.
    LogMetadata log_metadata;
  };

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

  // Adds a log to the list. |log_metadata| refers to metadata associated with
  // the log. Before being stored, the data will be compressed, and a hash and
  // signature will be computed.
  // TODO(crbug.com/40119012): Remove this function, and use StoreLogInfo()
  // everywhere instead.
  void StoreLog(const std::string& log_data,
                const LogMetadata& log_metadata,
                MetricsLogsEventManager::CreateReason reason);

  // Adds a log to the list, represented by a LogInfo object. This is useful
  // if the LogInfo instance needs to be created outside the main thread
  // (since creating a LogInfo from log data requires heavy work). Note that we
  // also pass the size of the log data before being compressed. This is simply
  // for calculating and emitting some metrics, and is otherwise unused.
  void StoreLogInfo(std::unique_ptr<LogInfo> log_info,
                    size_t uncompressed_log_size,
                    MetricsLogsEventManager::CreateReason reason);

  // Gets log data at the given index in the list.
  const std::string& GetLogAtIndex(size_t index);

  // Replaces the compressed log at |index| in the store with given log data and
  // |log_metadata| reusing the same timestamp.
  std::string ReplaceLogAtIndex(size_t index,
                                const std::string& new_log_data,
                                const LogMetadata& log_metadata);

  // Deletes all logs, in memory and on disk.
  void Purge();

  // Sets |logs_event_manager_|.
  void SetLogsEventManager(MetricsLogsEventManager* logs_event_manager);

  // Returns the timestamp of the element in the front of the list.
  const std::string& staged_log_timestamp() const;

  // The number of elements currently stored.
  size_t size() const { return list_.size(); }

  // The signing key used to compute the signature for a log.
  const std::string& signing_key() const { return signing_key_; }

  // Returns |logs_event_manager_|.
  MetricsLogsEventManager* GetLogsEventManagerForTesting() const {
    return logs_event_manager_;
  }

  // Computes the HMAC for |log_data| using the |signing_key| and returns a bool
  // indicating whether the signing succeeded. The returned HMAC is written to
  // the |signature|.
  static bool ComputeHMACForLog(const std::string& log_data,
                                const std::string& signing_key,
                                std::string* signature);

 private:
  FRIEND_TEST_ALL_PREFIXES(UnsentLogStoreTest, UnsentLogMetadataMetrics);

  // Reads the list of logs from |list|.
  void ReadLogsFromPrefList(const base::Value::List& list);

  // Writes the unsent log info to the |metadata_pref_name_| preference.
  void WriteToMetricsPref(base::HistogramBase::Count unsent_samples_count,
                          base::HistogramBase::Count sent_samples_count,
                          size_t persisted_size) const;

  // Records the info in |metadata_pref_name_| as UMA metrics.
  void RecordMetaDataMetrics();

  // Wrapper functions for the notify functions of |logs_event_manager_|.
  void NotifyLogCreated(const LogInfo& info,
                        MetricsLogsEventManager::CreateReason reason);
  void NotifyLogsCreated(base::span<std::unique_ptr<LogInfo>> logs,
                         MetricsLogsEventManager::CreateReason reason);
  void NotifyLogEvent(MetricsLogsEventManager::LogEvent event,
                      std::string_view log_hash,
                      std::string_view message = "");
  void NotifyLogsEvent(base::span<std::unique_ptr<LogInfo>> logs,
                       MetricsLogsEventManager::LogEvent event,
                       std::string_view message = "");

  // An object for recording UMA metrics.
  std::unique_ptr<UnsentLogStoreMetrics> metrics_;

  // A weak pointer to the PrefService object to read and write the preference
  // from.  Calling code should ensure this object continues to exist for the
  // lifetime of the UnsentLogStore object.
  raw_ptr<PrefService> local_state_;

  // The name of the preference to serialize logs to/from.
  const char* log_data_pref_name_;

  // The name of the preference to store the unsent logs info, it could be
  // nullptr if the metadata isn't desired.
  const char* metadata_pref_name_;

  const UnsentLogStoreLimits log_store_limits_;

  // Used to create a signature of log data, in order to verify reported data is
  // authentic.
  const std::string signing_key_;

  // Event manager to notify observers of log events.
  raw_ptr<MetricsLogsEventManager> logs_event_manager_;

  // A list of all of the stored logs, stored with SHA1 hashes to check for
  // corruption while they are stored in memory.
  std::vector<std::unique_ptr<LogInfo>> list_;

  // The index and type of the log staged for upload. If nothing has been
  // staged, the index will be -1.
  int staged_log_index_;

  // The total number of samples that have been sent from this LogStore.
  base::HistogramBase::Count total_samples_sent_ = 0;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_UNSENT_LOG_STORE_H_
