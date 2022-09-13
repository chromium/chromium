// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/unsent_log_store_metrics_impl.h"

#include "base/metrics/histogram_functions.h"

namespace metrics {

void UnsentLogStoreMetricsImpl::RecordCompressionRatio(size_t compressed_size,
                                                       size_t original_size) {
  base::UmaHistogramPercentageObsoleteDoNotUse(
      "UMA.ProtoCompressionRatio",
      static_cast<int>(100 * compressed_size / original_size));
}

void UnsentLogStoreMetricsImpl::RecordDroppedLogSize(size_t size) {
  base::UmaHistogramCounts1M("UMA.UnsentLogs.DroppedSize",
                             static_cast<int>(size));
}

void UnsentLogStoreMetricsImpl::RecordDroppedLogsNum(int dropped_logs_num) {
  base::UmaHistogramCounts1M("UMA.UnsentLogs.Dropped", dropped_logs_num);
}

void UnsentLogStoreMetricsImpl::RecordLastUnsentLogMetadataMetrics(
    int unsent_samples_count,
    int sent_samples_count,
    int persisted_size_in_kb) {
  if (!base::FeatureList::IsEnabled(kRecordLastUnsentLogMetadataMetrics))
    return;

  if (unsent_samples_count < 0 || sent_samples_count < 0 ||
      persisted_size_in_kb < 0) {
    return;
  }

  base::UmaHistogramCounts100000("UMA.UnsentLogs.UnsentCount",
                                 unsent_samples_count);
  base::UmaHistogramCounts1M("UMA.UnsentLogs.SentCount", sent_samples_count);
  // Sets 10MB as maximum because the total size of logs in each LogStore is up
  // to 6MB.
  base::UmaHistogramCounts10000("UMA.UnsentLogs.PersistedSizeInKB",
                                persisted_size_in_kb);

  if (sent_samples_count == 0 && unsent_samples_count == 0) {
    base::UmaHistogramPercentageObsoleteDoNotUse(
        "UMA.UnsentLogs.UnsentPercentage", 0);
  } else {
    base::UmaHistogramPercentageObsoleteDoNotUse(
        "UMA.UnsentLogs.UnsentPercentage",
        100 * unsent_samples_count /
            (unsent_samples_count + sent_samples_count));
  }
}

}  // namespace metrics
