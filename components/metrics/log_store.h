// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_LOG_STORE_H_
#define COMPONENTS_METRICS_LOG_STORE_H_

#include <optional>
#include <string>
#include <string_view>

#include "components/metrics/metrics_log.h"

namespace metrics {

// Specifies why a log store removed a log before a successful upload.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(UnsentLogDropReason)
enum class UnsentLogDropReason {
  // The trim removed the log because the queue reached both of its limits.
  kTrimmedQueueLimit = 0,
  // The trim removed the log because the log is larger than the size limit.
  kTrimmedLogTooLarge = 1,
  // The upload failed and the log is too large to send again.
  kUploadRejectedTooLarge = 2,
  // The server returned HTTP 400. A new attempt cannot succeed.
  kUploadBadRequest = 3,
  // The uploader asked to discard the log (for example, no upload URL).
  kForceDiscard = 4,
  // The user preference does not allow metrics upload for this log.
  kUserPreference = 5,
  // The persisted log list was corrupt at load time.
  kPrefCorruption = 6,
  // The store removed all logs (for example, consent turned off or cloned
  // install).
  kPurged = 7,
  kMaxValue = kPurged,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/uma/enums.xml:UnsentLogDropReason)

// Interface for local storage of serialized logs to be reported.
// It allows consumers to check if there are logs to consume, consume them one
// at a time by staging and discarding logs, and persist/load the whole set.
class LogStore {
 public:
  virtual ~LogStore() = default;

  // Returns true if there are any logs waiting to be uploaded.
  virtual bool has_unsent_logs() const = 0;

  // Returns true if there is a log that needs to be, or is being, uploaded.
  virtual bool has_staged_log() const = 0;

  // The text of the staged log, as a serialized protobuf.
  // Will trigger a DCHECK if there is no staged log.
  virtual const std::string& staged_log() const = 0;

  // The SHA1 hash of the staged log. This is used to detect log corruption.
  // Will trigger a DCHECK if there is no staged log.
  virtual const std::string& staged_log_hash() const = 0;

  // The HMAC-SHA256 signature of the staged log. This is used to validate that
  // a log originated from Chrome, and to detect corruption.
  // Will trigger a DCHECK if there is no staged log.
  virtual const std::string& staged_log_signature() const = 0;

  // User id associated with the staged log. Empty if the log was
  // recorded during no particular user session or during guest session.
  //
  // Will trigger a DCHECK if there is no staged log.
  virtual std::optional<uint64_t> staged_log_user_id() const = 0;

  // LogMetadata associated with the staged log.
  virtual const LogMetadata& staged_log_metadata() const = 0;

  // Populates staged_log() with the next stored log to send.
  // The order in which logs are staged is up to the implementor.
  // The staged_log must remain the same even if additional logs are added.
  // Should only be called if has_unsent_logs() is true.
  virtual void StageNextLog() = 0;

  // Discards the staged log. |reason| is the reason why the log was discarded
  // (used for debugging through chrome://metrics-internals).
  void DiscardStagedLog() { DiscardStagedLogImpl("", std::nullopt); }
  void DiscardStagedLog(std::string_view reason) {
    DiscardStagedLogImpl(reason, std::nullopt);
  }

  // Drops the staged log before a successful upload and records
  // `drop_reason` in the drop metrics. `reason` is a message for
  // chrome://metrics-internals.
  void DropUnsentStagedLog(std::string_view reason,
                           UnsentLogDropReason drop_reason) {
    DiscardStagedLogImpl(reason, drop_reason);
  }

 protected:
  // Discards the staged log. Records `drop_reason` in the drop metrics when
  // `drop_reason` has a value (empty when popping an uploaded log).
  virtual void DiscardStagedLogImpl(
      std::string_view reason,
      std::optional<UnsentLogDropReason> drop_reason) = 0;

 public:
  // Marks the staged log as sent, DiscardStagedLog() shall still be called if
  // the staged log needs discarded.
  virtual void MarkStagedLogAsSent() = 0;

  // Trims saved logs and writes them to persistent storage. When
  // |overwrite_in_memory_store| is false, we will still not persist logs that
  // should be trimmed away, but they will still be available in memory
  // (allowing them to still be eligible for upload this session).
  // TODO(crbug.com/40745324): Revisit call sites and determine what value of
  // |overwrite_in_memory_store| they should use.
  virtual void TrimAndPersistUnsentLogs(bool overwrite_in_memory_store) = 0;

  // Loads unsent logs from persistent storage.
  virtual void LoadPersistedUnsentLogs() = 0;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_LOG_STORE_H_
