// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_UNSENT_LOG_STORE_METRICS_IMPL_H_
#define COMPONENTS_METRICS_UNSENT_LOG_STORE_METRICS_IMPL_H_

#include <cstddef>
#include <string>
#include <string_view>

#include "components/metrics/log_store.h"
#include "components/metrics/unsent_log_store_metrics.h"

namespace metrics {

// Implementation for recording metrics from UnsentLogStore.
class UnsentLogStoreMetricsImpl : public UnsentLogStoreMetrics {
 public:
  // Identifies the UMA log queue that owns the UnsentLogStore. The value
  // selects the {LogQueue} variant of the per-queue histograms.
  // LINT.IfChange(UnsentLogQueue)
  enum class LogQueue {
    // The queue of initial stability logs.
    kInitial,
    // The queue of ongoing logs.
    kOngoing,
    // The ChromeOS per-user queue of ongoing logs.
    kUser,
  };
  // LINT.ThenChange(//components/metrics/unsent_log_store_metrics_impl.cc:UnsentLogQueue,//tools/metrics/histograms/metadata/uma/histograms.xml:UnsentLogQueue)

  explicit UnsentLogStoreMetricsImpl(LogQueue log_queue);

  UnsentLogStoreMetricsImpl(const UnsentLogStoreMetricsImpl&) = delete;
  UnsentLogStoreMetricsImpl& operator=(const UnsentLogStoreMetricsImpl&) =
      delete;

  ~UnsentLogStoreMetricsImpl() override = default;

  // UnsentLogStoreMetrics:
  void RecordLogReadStatus(LogReadStatus status) override;
  void RecordCompressionRatio(size_t compressed_size,
                              size_t original_size) override;
  void RecordDroppedLogs(UnsentLogDropReason drop_reason,
                         size_t count) override;
  void RecordLogsTrimmedOnDiskOnly(int trimmed_logs_count) override;
  void RecordDroppedLogSize(size_t size) override;
  void RecordDroppedLogsNum(int dropped_logs_num) override;
  void RecordLastUnsentLogMetadataMetrics(int unsent_samples_count,
                                          int sent_samples_count,
                                          int persisted_size_in_kb) override;

 private:
  // Returns the full name of the per-queue histogram `metric_name`.
  std::string GetQueueHistogramName(std::string_view metric_name) const;

  const LogQueue log_queue_;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_UNSENT_LOG_STORE_METRICS_IMPL_H_
