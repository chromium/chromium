// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_UNSENT_LOG_STORE_METRICS_IMPL_H_
#define COMPONENTS_METRICS_UNSENT_LOG_STORE_METRICS_IMPL_H_

#include "components/metrics/unsent_log_store_metrics.h"

namespace metrics {

// Implementation for recording metrics from UnsentLogStore.
class UnsentLogStoreMetricsImpl : public UnsentLogStoreMetrics {
 public:
  UnsentLogStoreMetricsImpl() = default;

  UnsentLogStoreMetricsImpl(const UnsentLogStoreMetricsImpl&) = delete;
  UnsentLogStoreMetricsImpl& operator=(const UnsentLogStoreMetricsImpl&) =
      delete;

  ~UnsentLogStoreMetricsImpl() override = default;

  // UnsentLogStoreMetrics:
  void RecordCompressionRatio(
    size_t compressed_size, size_t original_size) override;
  void RecordDroppedLogSize(size_t size) override;
  void RecordDroppedLogsNum(int dropped_logs_num) override;
  void RecordLastUnsentLogMetadataMetrics(int unsent_samples_count,
                                          int sent_samples_count,
                                          int persisted_size_in_kb) override;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_UNSENT_LOG_STORE_METRICS_IMPL_H_
