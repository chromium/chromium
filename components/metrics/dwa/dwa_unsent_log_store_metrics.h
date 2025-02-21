// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_DWA_DWA_UNSENT_LOG_STORE_METRICS_H_
#define COMPONENTS_METRICS_DWA_DWA_UNSENT_LOG_STORE_METRICS_H_

#include "components/metrics/unsent_log_store_metrics.h"

namespace metrics::dwa {

// Implementation for recording metrics from UnsentLogStore.
class DwaUnsentLogStoreMetrics : public metrics::UnsentLogStoreMetrics {
 public:
  DwaUnsentLogStoreMetrics();

  DwaUnsentLogStoreMetrics(const DwaUnsentLogStoreMetrics&) = delete;
  DwaUnsentLogStoreMetrics& operator=(const DwaUnsentLogStoreMetrics&) = delete;

  ~DwaUnsentLogStoreMetrics() override;

  // metrics::UnsentLogStoreMetrics:
  void RecordLogReadStatus(
      metrics::UnsentLogStoreMetrics::LogReadStatus status) override;
  void RecordCompressionRatio(size_t compressed_size,
                              size_t original_size) override;
  void RecordDroppedLogSize(size_t size) override;
  void RecordDroppedLogsNum(int dropped_logs_num) override;
};

}  // namespace metrics::dwa

#endif  // COMPONENTS_METRICS_DWA_DWA_UNSENT_LOG_STORE_METRICS_H_
