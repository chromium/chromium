// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/unsent_log_store_metrics.h"

namespace metrics {

// static
BASE_FEATURE(kRecordLastUnsentLogMetadataMetrics,
             "RecordLastUnsentLogMetadataMetrics",
             base::FEATURE_DISABLED_BY_DEFAULT);

UnsentLogStoreMetrics::UnsentLogStoreMetrics() = default;
UnsentLogStoreMetrics::~UnsentLogStoreMetrics() = default;

void UnsentLogStoreMetrics::RecordLogReadStatus(LogReadStatus status) {}

void UnsentLogStoreMetrics::RecordCompressionRatio(size_t compressed_size,
                                                   size_t original_size) {}

void UnsentLogStoreMetrics::RecordDroppedLogSize(size_t size) {}

void UnsentLogStoreMetrics::RecordDroppedLogsNum(int dropped_logs_num) {}

void UnsentLogStoreMetrics::RecordLastUnsentLogMetadataMetrics(
    int unsent_samples_count,
    int sent_samples_count,
    int persisted_size_in_kb) {}

}  // namespace metrics
