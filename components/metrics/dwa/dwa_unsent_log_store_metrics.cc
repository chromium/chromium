// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/dwa/dwa_unsent_log_store_metrics.h"

#include "base/metrics/histogram_macros.h"

namespace metrics::dwa {

DwaUnsentLogStoreMetrics::DwaUnsentLogStoreMetrics() = default;

DwaUnsentLogStoreMetrics::~DwaUnsentLogStoreMetrics() = default;

void DwaUnsentLogStoreMetrics::RecordLogReadStatus(
    metrics::UnsentLogStoreMetrics::LogReadStatus status) {
  UMA_HISTOGRAM_ENUMERATION("DWA.PersistentLogRecall.Status", status,
                            metrics::UnsentLogStoreMetrics::END_RECALL_STATUS);
}

void DwaUnsentLogStoreMetrics::RecordCompressionRatio(size_t compressed_size,
                                                      size_t original_size) {
  UMA_HISTOGRAM_PERCENTAGE(
      "DWA.ProtoCompressionRatio",
      static_cast<int>(100 * compressed_size / original_size));
}

void DwaUnsentLogStoreMetrics::RecordDroppedLogSize(size_t size) {
  UMA_HISTOGRAM_COUNTS_1M("DWA.UnsentLogs.DroppedSize", static_cast<int>(size));
}

void DwaUnsentLogStoreMetrics::RecordDroppedLogsNum(int dropped_logs_num) {
  UMA_HISTOGRAM_COUNTS_10000("DWA.UnsentLogs.NumDropped", dropped_logs_num);
}

}  // namespace metrics::dwa
