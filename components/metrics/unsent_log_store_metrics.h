// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_UNSENT_LOG_STORE_METRICS_H_
#define COMPONENTS_METRICS_UNSENT_LOG_STORE_METRICS_H_

#include <cstddef>
#include <optional>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "components/metrics/log_store.h"
#include "components/metrics/unsent_log_store.h"

namespace metrics {

// The feature to record the unsent log info metrics, refer to
// UnsentLogStoreMetricsImpl::RecordLastUnsentLogMetadataMetrics.
BASE_DECLARE_FEATURE(kRecordLastUnsentLogMetadataMetrics);

// Interface for recording metrics from UnsentLogStore.
class UnsentLogStoreMetrics {
 public:
  // Used to produce a histogram that keeps track of the status of recalling
  // persisted per logs.
  enum LogReadStatus {
    RECALL_SUCCESS,         // We were able to correctly recall a persisted log.
    LIST_EMPTY,             // Attempting to recall from an empty list.
    LIST_SIZE_MISSING,      // Failed to recover list size using GetAsInteger().
    LIST_SIZE_TOO_SMALL,    // Too few elements in the list (less than 3).
    LIST_SIZE_CORRUPTION,   // List size is not as expected.
    LOG_STRING_CORRUPTION,  // Failed to recover log string using GetAsString().
    CHECKSUM_CORRUPTION,    // Failed to verify checksum.
    CHECKSUM_STRING_CORRUPTION,     // Failed to recover checksum string using
                                    // GetAsString().
    DECODE_FAIL,                    // Failed to decode log.
    DEPRECATED_XML_PROTO_MISMATCH,  // The XML and protobuf logs have
                                    // inconsistent data.
    kMaxValue = DEPRECATED_XML_PROTO_MISMATCH
  };

  UnsentLogStoreMetrics();

  UnsentLogStoreMetrics(const UnsentLogStoreMetrics&) = delete;
  UnsentLogStoreMetrics& operator=(const UnsentLogStoreMetrics&) = delete;

  virtual ~UnsentLogStoreMetrics();

  virtual void RecordLogReadStatus(LogReadStatus status);

  virtual void RecordCompressionRatio(size_t compressed_size,
                                      size_t original_size);

  // Records `count` logs that the store removed before a successful upload
  // because of `drop_reason`. Does nothing if `count` is 0.
  virtual void RecordDroppedLogs(UnsentLogDropReason drop_reason, size_t count);
  void RecordDroppedLog(UnsentLogDropReason drop_reason) {
    RecordDroppedLogs(drop_reason, 1);
  }

  // Records a corrupt persisted log list of `dropped_logs_count` entries.
  void RecordCorruptLogRead(size_t dropped_logs_count);

  // Records the outcome of a trim pass. Each set entry in `drop_reasons` gives
  // the reason why that log was trimmed, while empty entries were kept. When
  // `removed_from_memory` is false, the trimmed logs stay in memory and can
  // still upload in this session.
  void RecordTrimmedLogs(
      base::span<const std::optional<UnsentLogDropReason>> drop_reasons,
      bool removed_from_memory);

  // Records the number of logs that a trim removed from persistent storage
  // but kept in memory, so that they can still upload in this session.
  virtual void RecordLogsTrimmedOnDiskOnly(int trimmed_logs_count);

  virtual void RecordDroppedLogSize(size_t size);

  virtual void RecordDroppedLogsNum(int dropped_logs_num);

  virtual void RecordLastUnsentLogMetadataMetrics(int unsent_samples_count,
                                                  int sent_samples_count,
                                                  int persisted_size_in_kb);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_UNSENT_LOG_STORE_METRICS_H_
