// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_UNSENT_LOG_STORE_METRICS_IMPL_H_
#define COMPONENTS_UKM_UNSENT_LOG_STORE_METRICS_IMPL_H_

#include "components/metrics/unsent_log_store_metrics.h"

namespace ukm {

// Implementation for recording metrics from UnsentLogStore.
class UnsentLogStoreMetricsImpl : public metrics::UnsentLogStoreMetrics {
 public:
  UnsentLogStoreMetricsImpl();

  UnsentLogStoreMetricsImpl(const UnsentLogStoreMetricsImpl&) = delete;
  UnsentLogStoreMetricsImpl& operator=(const UnsentLogStoreMetricsImpl&) =
      delete;

  ~UnsentLogStoreMetricsImpl() override;

  // metrics::UnsentLogStoreMetrics:
  void RecordLogReadStatus(
      metrics::UnsentLogStoreMetrics::LogReadStatus status) override;
  void RecordCompressionRatio(size_t compressed_size,
                              size_t original_size) override;
  void RecordDroppedLogSize(size_t size) override;
  void RecordDroppedLogsNum(int dropped_logs_num) override;
};

}  // namespace ukm

#endif  // COMPONENTS_UKM_UNSENT_LOG_STORE_METRICS_IMPL_H_
