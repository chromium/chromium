// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/unsent_log_store_metrics_impl.h"

#include <cstdint>

#include "base/check_op.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"

namespace metrics {

namespace {

// Returns the {LogQueue} variant name for `log_queue`.
// LINT.IfChange(UnsentLogQueue)
std::string_view GetLogQueueName(
    UnsentLogStoreMetricsImpl::LogQueue log_queue) {
  switch (log_queue) {
    case UnsentLogStoreMetricsImpl::LogQueue::kInitial:
      return "Initial";
    case UnsentLogStoreMetricsImpl::LogQueue::kOngoing:
      return "Ongoing";
    case UnsentLogStoreMetricsImpl::LogQueue::kUser:
      return "User";
  }
  NOTREACHED();
}
// LINT.ThenChange(//components/metrics/unsent_log_store_metrics_impl.h:UnsentLogQueue,//tools/metrics/histograms/metadata/uma/histograms.xml:UnsentLogQueue)

}  // namespace

UnsentLogStoreMetricsImpl::UnsentLogStoreMetricsImpl(LogQueue log_queue)
    : log_queue_(log_queue) {}

void UnsentLogStoreMetricsImpl::RecordLogReadStatus(LogReadStatus status) {
  base::UmaHistogramEnumeration(GetQueueHistogramName("LogReadStatus"), status);
}

void UnsentLogStoreMetricsImpl::RecordCompressionRatio(size_t compressed_size,
                                                       size_t original_size) {
  base::UmaHistogramPercentageObsoleteDoNotUse(
      "UMA.ProtoCompressionRatio",
      static_cast<int>(100 * compressed_size / original_size));
}

void UnsentLogStoreMetricsImpl::RecordDroppedLogs(
    UnsentLogDropReason drop_reason,
    size_t count) {
  if (count == 0) {
    return;
  }
  constexpr int kExclusiveMax =
      static_cast<int>(UnsentLogDropReason::kMaxValue) + 1;
  static_assert(kExclusiveMax < base::LinearHistogram::kBucketCount_MAX);
  DCHECK_LE(static_cast<uintmax_t>(drop_reason),
            static_cast<uintmax_t>(UnsentLogDropReason::kMaxValue));
  base::HistogramBase* histogram = base::LinearHistogram::FactoryGet(
      GetQueueHistogramName("DropReason"), /*minimum=*/1, kExclusiveMax,
      static_cast<size_t>(kExclusiveMax + 1),
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->AddCount(static_cast<int>(drop_reason),
                      base::saturated_cast<int>(count));
}

void UnsentLogStoreMetricsImpl::RecordLogsTrimmedOnDiskOnly(
    int trimmed_logs_count) {
  base::UmaHistogramCounts100(GetQueueHistogramName("TrimmedOnDiskOnly"),
                              trimmed_logs_count);
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
  if (!base::FeatureList::IsEnabled(kRecordLastUnsentLogMetadataMetrics)) {
    return;
  }

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

std::string UnsentLogStoreMetricsImpl::GetQueueHistogramName(
    std::string_view metric_name) const {
  return base::StrCat(
      {"UMA.UnsentLogs.", GetLogQueueName(log_queue_), ".", metric_name});
}

}  // namespace metrics
