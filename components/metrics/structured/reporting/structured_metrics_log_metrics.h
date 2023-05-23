// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_REPORTING_STRUCTURED_METRICS_LOG_METRICS_H_
#define COMPONENTS_METRICS_STRUCTURED_REPORTING_STRUCTURED_METRICS_LOG_METRICS_H_

#include "components/metrics/unsent_log_store_metrics.h"

namespace metrics::structured::reporting {

class StructuredMetricsLogMetrics : public UnsentLogStoreMetrics {
 public:
  StructuredMetricsLogMetrics() = default;

  ~StructuredMetricsLogMetrics() override = default;

  void RecordCompressionRatio(size_t compressed_size,
                              size_t original_size) override;

  void RecordDroppedLogSize(size_t size) override;

  void RecordDroppedLogsNum(int dropped_logs_num) override;
};

}  // namespace metrics::structured::reporting

#endif  // COMPONENTS_METRICS_STRUCTURED_REPORTING_STRUCTURED_METRICS_LOG_METRICS_H_
