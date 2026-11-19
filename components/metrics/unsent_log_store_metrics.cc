// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/unsent_log_store_metrics.h"

#include "base/numerics/clamped_math.h"
#include "base/numerics/safe_conversions.h"

namespace metrics {

// static
BASE_FEATURE(kRecordLastUnsentLogMetadataMetrics,
             base::FEATURE_DISABLED_BY_DEFAULT);

UnsentLogStoreMetrics::UnsentLogStoreMetrics() = default;
UnsentLogStoreMetrics::~UnsentLogStoreMetrics() = default;

void UnsentLogStoreMetrics::RecordLogReadStatus(LogReadStatus status) {}

void UnsentLogStoreMetrics::RecordCompressionRatio(size_t compressed_size,
                                                   size_t original_size) {}

void UnsentLogStoreMetrics::RecordDroppedLogs(UnsentLogDropReason drop_reason,
                                              size_t count) {}

void UnsentLogStoreMetrics::RecordCorruptLogRead(size_t dropped_logs_count) {
  RecordLogReadStatus(LOG_STRING_CORRUPTION);
  RecordDroppedLogs(UnsentLogDropReason::kPrefCorruption, dropped_logs_count);
}

void UnsentLogStoreMetrics::RecordTrimmedLogs(size_t queue_limit_count,
                                              size_t log_too_large_count,
                                              bool removed_from_memory) {
  const size_t total_trimmed =
      base::ClampAdd(queue_limit_count, log_too_large_count);
  if (total_trimmed == 0) {
    return;
  }
  if (!removed_from_memory) {
    RecordLogsTrimmedOnDiskOnly(base::saturated_cast<int>(total_trimmed));
    return;
  }
  RecordDroppedLogs(UnsentLogDropReason::kTrimmedQueueLimit, queue_limit_count);
  RecordDroppedLogs(UnsentLogDropReason::kTrimmedLogTooLarge,
                    log_too_large_count);
  RecordDroppedLogsNum(base::saturated_cast<int>(total_trimmed));
}

void UnsentLogStoreMetrics::RecordLogsTrimmedOnDiskOnly(
    int trimmed_logs_count) {}

void UnsentLogStoreMetrics::RecordDroppedLogSize(size_t size) {}

void UnsentLogStoreMetrics::RecordDroppedLogsNum(int dropped_logs_num) {}

void UnsentLogStoreMetrics::RecordLastUnsentLogMetadataMetrics(
    int unsent_samples_count,
    int sent_samples_count,
    int persisted_size_in_kb) {}

}  // namespace metrics
