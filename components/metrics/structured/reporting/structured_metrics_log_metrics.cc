// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/reporting/structured_metrics_log_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace metrics::structured::reporting {
void StructuredMetricsLogMetrics::RecordCompressionRatio(size_t compressed_size,
                                                         size_t original_size) {
  CHECK(original_size != 0);
  double ratio = static_cast<double>(compressed_size) / original_size;
  base::UmaHistogramPercentage("StructuredMetrics.LogStore.CompressionRatio",
                               static_cast<int>(100 * ratio));
}

void StructuredMetricsLogMetrics::RecordDroppedLogSize(size_t size) {
  const size_t size_kb = size / 1024;
  base::UmaHistogramCounts10000("StructuredMetrics.LogStore.DroppedSize",
                                size_kb);
}

void StructuredMetricsLogMetrics::RecordDroppedLogsNum(int dropped_logs_num) {
  base::UmaHistogramCounts100000("StructuredMetrics.LogStore.Dropped",
                                 dropped_logs_num);
}

}  // namespace metrics::structured::reporting
