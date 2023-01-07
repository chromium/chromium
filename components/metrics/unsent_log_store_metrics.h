// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_UNSENT_LOG_STORE_METRICS_H_
#define COMPONENTS_METRICS_UNSENT_LOG_STORE_METRICS_H_

#include "base/feature_list.h"
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
    END_RECALL_STATUS  // Number of bins to use to create the histogram.
  };

  UnsentLogStoreMetrics();

  UnsentLogStoreMetrics(const UnsentLogStoreMetrics&) = delete;
  UnsentLogStoreMetrics& operator=(const UnsentLogStoreMetrics&) = delete;

  virtual ~UnsentLogStoreMetrics();

  virtual void RecordLogReadStatus(LogReadStatus status);

  virtual void RecordCompressionRatio(size_t compressed_size,
                                      size_t original_size);

  virtual void RecordDroppedLogSize(size_t size);

  virtual void RecordDroppedLogsNum(int dropped_logs_num);

  virtual void RecordLastUnsentLogMetadataMetrics(int unsent_samples_count,
                                                  int sent_samples_count,
                                                  int persisted_size_in_kb);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_UNSENT_LOG_STORE_METRICS_H_
