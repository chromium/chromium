// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/unsent_log_store_metrics_impl.h"

#include "base/metrics/histogram_macros.h"

namespace ukm {

UnsentLogStoreMetricsImpl::UnsentLogStoreMetricsImpl() = default;

UnsentLogStoreMetricsImpl::~UnsentLogStoreMetricsImpl() = default;

void UnsentLogStoreMetricsImpl::RecordLogReadStatus(
    metrics::UnsentLogStoreMetrics::LogReadStatus status) {
  UMA_HISTOGRAM_ENUMERATION("UKM.PersistentLogRecall.Status", status,
                            metrics::UnsentLogStoreMetrics::END_RECALL_STATUS);
}

void UnsentLogStoreMetricsImpl::RecordCompressionRatio(size_t compressed_size,
                                                      size_t original_size) {
  UMA_HISTOGRAM_PERCENTAGE(
      "UKM.ProtoCompressionRatio",
      static_cast<int>(100 * compressed_size / original_size));
}

void UnsentLogStoreMetricsImpl::RecordDroppedLogSize(size_t size) {
  UMA_HISTOGRAM_COUNTS_1M("UKM.UnsentLogs.DroppedSize", static_cast<int>(size));
}

void UnsentLogStoreMetricsImpl::RecordDroppedLogsNum(int dropped_logs_num) {
  UMA_HISTOGRAM_COUNTS_10000("UKM.UnsentLogs.NumDropped", dropped_logs_num);
}

}  // namespace ukm
