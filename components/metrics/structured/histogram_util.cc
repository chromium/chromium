// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/histogram_util.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"

namespace metrics::structured {

void LogEventRecordingState(EventRecordingState state) {
  base::UmaHistogramEnumeration("UMA.StructuredMetrics.EventRecordingState2",
                                state);
}

void LogNumEventsInUpload(const int num_events) {
  UMA_HISTOGRAM_COUNTS_1000("UMA.StructuredMetrics.NumEventsInUpload",
                            num_events);
}

void LogNumEventsRecordedBeforeInit(int num_events) {
  UMA_HISTOGRAM_COUNTS_100("UMA.StructuredMetrics.EventsRecordedBeforeInit",
                           num_events);
}

void LogNumFilesPerExternalMetricsScan(int num_files) {
  base::UmaHistogramCounts1000(
      "UMA.StructuredMetrics.NumFilesPerExternalMetricsScan", num_files);
}

void LogEventFileSizeKB(int64_t file_size_kb) {
  base::UmaHistogramMemoryKB("UMA.StructuredMetrics.EventFileSize",
                             file_size_kb);
}
void LogEventSerializedSizeBytes(int64_t event_size_bytes) {
  base::UmaHistogramCounts1000("UMA.StructuredMetrics.EventSerializedSize",
                               event_size_bytes);
}

void LogUploadSizeBytes(int64_t upload_size_bytes) {
  base::UmaHistogramCounts100000("StructuredMetrics.UploadSize",
                                 upload_size_bytes);
}

void LogExternalMetricsScanInUpload(int num_scans) {
  base::UmaHistogramExactLinear(
      "UMA.StructuredMetrics.ExternalMetricScansPerUpload", num_scans, 10);
}

void LogDroppedExternalMetrics(int num_dropped) {
  base::UmaHistogramCounts1000("StructuredMetrics.ExternalMetricsDropped",
                               num_dropped);
}

void LogDroppedProjectExternalMetrics(std::string_view project_name,
                                      int num_dropped) {
  const std::string histogram_name =
      base::StrCat({kExternalMetricsDroppedHistogramPrefix, project_name});
  base::UmaHistogramCounts100(histogram_name, num_dropped);
}

void LogProducedProjectExternalMetrics(std::string_view project_name,
                                       int num_produced) {
  const std::string histogram_name =
      base::StrCat({kExternalMetricsProducedHistogramPrefix, project_name});
  base::UmaHistogramCounts100(histogram_name, num_produced);
}

void LogStorageManagerFlushStatus(StorageManagerFlushStatus status) {
  base::UmaHistogramEnumeration("StructuredMetrics.StorageManager.Flush",
                                status);
}

void LogFlushedMapError(FlushedMapError error) {
  base::UmaHistogramEnumeration("StructuredMetrics.FlushedMap.Error", error);
}

void LogFlushedMapLoadedFlushedKeys(int count) {
  base::UmaHistogramCounts100("StructuredMetrics.FlushedMap.LoadedFlushedKey",
                              count);
}

void LogDeletedBuffersWhenOverQuota(int count) {
  base::UmaHistogramCounts100(
      "StructuredMetrics.StorageManager.DeletedBuffersWhenOverQuota", count);
}

void LogDiskQuotaExceededDelta(int delta_kb) {
  base::UmaHistogramCounts100(
      "StructuredMetrics.StorageManager.DiskQuotaExceededDelta", delta_kb);
}

void LogFlushedBuffersAtUpload(int count) {
  base::UmaHistogramCounts100(
      "StructuredMetrics.StorageManager.FlushedBuffersAtUpload", count);
}

void LogInMemoryEventsAtUpload(int count) {
  base::UmaHistogramCounts100(
      "StructuredMetrics.StorageManager.InMemoryEventsAtUpload", count);
}

void LogMaxDiskSizeKb(int size_kb) {
  base::UmaHistogramCounts100("StructuredMetrics.StorageManager.MaxDiskSizeKB",
                              size_kb);
}

void LogMaxMemorySizeKb(int size_kb) {
  base::UmaHistogramCounts100(
      "StructuredMetrics.StorageManager.MaxMemorySizeKB", size_kb);
}

void LogStorageManagerRecordStatus(RecordStatus status) {
  base::UmaHistogramEnumeration("StructuredMetrics.StorageManager.RecordStatus",
                                status);
}

}  // namespace metrics::structured
