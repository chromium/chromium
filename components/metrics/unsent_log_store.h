// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_UNSENT_LOG_STORE_H_
#define COMPONENTS_METRICS_UNSENT_LOG_STORE_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/values.h"
#include "components/metrics/log_store.h"

class PrefService;

namespace metrics {

class UnsentLogStoreMetrics;

// Maintains a list of unsent logs that are written and restored from disk.
class UnsentLogStore : public LogStore {
 public:
  // Constructs an UnsentLogStore that stores data in |local_state| under the
  // preference |pref_name|.
  // Calling code is responsible for ensuring that the lifetime of |local_state|
  // is longer than the lifetime of UnsentLogStore.
  //
  // When saving logs to disk, stores either the first |min_log_count| logs, or
  // at least |min_log_bytes| bytes of logs, whichever is greater.
  //
  // If the optional |max_log_size| parameter is non-zero, all logs larger than
  // that limit will be skipped when writing to disk.
  //
  // |signing_key| is used to produce an HMAC-SHA256 signature of the logged
  // data, which will be uploaded with the log and used to validate data
  // integrity.
  UnsentLogStore(std::unique_ptr<UnsentLogStoreMetrics> metrics,
                PrefService* local_state,
                const char* pref_name,
                size_t min_log_count,
                size_t min_log_bytes,
                size_t max_log_size,
                const std::string& signing_key);
  ~UnsentLogStore();

  // LogStore:
  bool has_unsent_logs() const override;
  bool has_staged_log() const override;
  const std::string& staged_log() const override;
  const std::string& staged_log_hash() const override;
  const std::string& staged_log_signature() const override;
  void StageNextLog() override;
  void DiscardStagedLog() override;
  void PersistUnsentLogs() const override;
  void LoadPersistedUnsentLogs() override;

  // Adds a log to the list.
  void StoreLog(const std::string& log_data);

  // Gets log data at the given index in the list.
  const std::string& GetLogAtIndex(size_t index);

  // Replaces the compressed log at |index| in the store with given log data
  // reusing the same timestamp from the original log, and returns old log data.
  std::string ReplaceLogAtIndex(size_t index, const std::string& new_log_data);

  // Deletes all logs, in memory and on disk.
  void Purge();

  // Returns the timestamp of the element in the front of the list.
  const std::string& staged_log_timestamp() const;

  // The number of elements currently stored.
  size_t size() const { return list_.size(); }

 private:
  // Writes the list to the ListValue.
  void WriteLogsToPrefList(base::ListValue* list) const;

  // Reads the list from the ListValue.
  void ReadLogsFromPrefList(const base::ListValue& list);

  // An object for recording UMA metrics.
  std::unique_ptr<UnsentLogStoreMetrics> metrics_;

  // A weak pointer to the PrefService object to read and write the preference
  // from.  Calling code should ensure this object continues to exist for the
  // lifetime of the UnsentLogStore object.
  PrefService* local_state_;

  // The name of the preference to serialize logs to/from.
  const char* pref_name_;

  // We will keep at least this |min_log_count_| logs or |min_log_bytes_| bytes
  // of logs, whichever is greater, when writing to disk.  These apply after
  // skipping logs greater than |max_log_size_|.
  const size_t min_log_count_;
  const size_t min_log_bytes_;

  // Logs greater than this size will not be written to disk.
  const size_t max_log_size_;

  // Used to create a signature of log data, in order to verify reported data is
  // authentic.
  const std::string signing_key_;

  struct LogInfo {
    LogInfo();
    LogInfo(const LogInfo& other);
    ~LogInfo();

    // Initializes the members based on uncompressed |log_data|,
    // |log_timestamp|, and |signing_key|. |log_data| is the uncompressed
    // serialized log protobuf. A hash and a signature are computed from
    // |log_data|. The signature is produced using |signing_key|. |log_data|
    // will be compressed and stored in |compressed_log_data|. |log_timestamp|
    // is stored as is.
    // |metrics| is the parent's metrics_ object, and should not be held.
    void Init(UnsentLogStoreMetrics* metrics,
              const std::string& log_data,
              const std::string& log_timestamp,
              const std::string& signing_key);

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
  };
  // A list of all of the stored logs, stored with SHA1 hashes to check for
  // corruption while they are stored in memory.
  std::vector<LogInfo> list_;

  // The index and type of the log staged for upload. If nothing has been
  // staged, the index will be -1.
  int staged_log_index_;

  DISALLOW_COPY_AND_ASSIGN(UnsentLogStore);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_UNSENT_LOG_STORE_H_
