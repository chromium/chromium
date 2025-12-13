// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_METRICS_UNSENT_LOG_STORE_METRICS_H_
#define COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_METRICS_UNSENT_LOG_STORE_METRICS_H_

#include "components/metrics/unsent_log_store_metrics.h"

namespace metrics::private_metrics {

// Implementation for recording metrics from UnsentLogStore.
class PrivateMetricsUnsentLogStoreMetrics
    : public metrics::UnsentLogStoreMetrics {
 public:
  PrivateMetricsUnsentLogStoreMetrics();

  PrivateMetricsUnsentLogStoreMetrics(
      const PrivateMetricsUnsentLogStoreMetrics&) = delete;
  PrivateMetricsUnsentLogStoreMetrics& operator=(
      const PrivateMetricsUnsentLogStoreMetrics&) = delete;

  ~PrivateMetricsUnsentLogStoreMetrics() override;

  // metrics::UnsentLogStoreMetrics:
  void RecordLogReadStatus(
      metrics::UnsentLogStoreMetrics::LogReadStatus status) override;
  void RecordCompressionRatio(size_t compressed_size,
                              size_t original_size) override;
  void RecordDroppedLogSize(size_t size) override;
  void RecordDroppedLogsNum(int dropped_logs_num) override;
};

}  // namespace metrics::private_metrics

#endif  // COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_METRICS_UNSENT_LOG_STORE_METRICS_H_
