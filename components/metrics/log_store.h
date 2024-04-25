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
  virtual const LogMetadata staged_log_metadata() const = 0;

  // Populates staged_log() with the next stored log to send.
  // The order in which logs are staged is up to the implementor.
  // The staged_log must remain the same even if additional logs are added.
  // Should only be called if has_unsent_logs() is true.
  virtual void StageNextLog() = 0;

  // Discards the staged log. |reason| is the reason why the log was discarded
  // (used for debugging through chrome://metrics-internals).
  virtual void DiscardStagedLog(std::string_view reason = "") = 0;

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
