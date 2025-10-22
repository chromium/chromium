// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/private_metrics/private_metrics_unsent_log_store_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "components/metrics/dwa/dwa_recorder.h"
#include "components/metrics/private_metrics/private_metrics_features.h"

namespace metrics::private_metrics {

PrivateMetricsUnsentLogStoreMetrics::PrivateMetricsUnsentLogStoreMetrics() =
    default;

PrivateMetricsUnsentLogStoreMetrics::~PrivateMetricsUnsentLogStoreMetrics() =
    default;

void PrivateMetricsUnsentLogStoreMetrics::RecordLogReadStatus(
    metrics::UnsentLogStoreMetrics::LogReadStatus status) {
  if (base::FeatureList::IsEnabled(private_metrics::kPrivateMetricsFeature)) {
    base::UmaHistogramEnumeration("PrivateMetrics.PersistentLogRecall.Status",
                                  status);
  } else {
    base::UmaHistogramEnumeration("DWA.PersistentLogRecall.Status", status);
  }
}

void PrivateMetricsUnsentLogStoreMetrics::RecordCompressionRatio(
    size_t compressed_size,
    size_t original_size) {
  if (base::FeatureList::IsEnabled(private_metrics::kPrivateMetricsFeature)) {
    base::UmaHistogramPercentage(
        "PrivateMetrics.ProtoCompressionRatio",
        static_cast<int>(100 * compressed_size / original_size));
  } else {
    base::UmaHistogramPercentage(
        "DWA.ProtoCompressionRatio",
        static_cast<int>(100 * compressed_size / original_size));
  }
}

void PrivateMetricsUnsentLogStoreMetrics::RecordDroppedLogSize(size_t size) {
  if (base::FeatureList::IsEnabled(private_metrics::kPrivateMetricsFeature)) {
    base::UmaHistogramCounts1M("PrivateMetrics.UnsentLogs.DroppedSize",
                               static_cast<int>(size));
  } else {
    base::UmaHistogramCounts1M("DWA.UnsentLogs.DroppedSize",
                               static_cast<int>(size));
  }
}

void PrivateMetricsUnsentLogStoreMetrics::RecordDroppedLogsNum(
    int dropped_logs_num) {
  if (base::FeatureList::IsEnabled(private_metrics::kPrivateMetricsFeature)) {
    base::UmaHistogramCounts10000("PrivateMetrics.UnsentLogs.NumDropped",
                                  dropped_logs_num);
  } else {
    base::UmaHistogramCounts10000("DWA.UnsentLogs.NumDropped",
                                  dropped_logs_num);
  }
}

}  // namespace metrics::private_metrics
