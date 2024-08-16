// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_HISTOGRAM_UTIL_H_
#define COMPONENTS_METRICS_STRUCTURED_HISTOGRAM_UTIL_H_

#include <string_view>

#include "components/prefs/persistent_pref_store.h"

namespace metrics::structured {

// Whether a single event was recorded correctly, or otherwise what error state
// occurred. These values are persisted to logs. Entries should not be
// renumbered and numeric values should never be reused.
enum class EventRecordingState {
  kRecorded = 0,
  kProviderUninitialized = 1,
  kRecordingDisabled = 2,
  kProviderMissing = 3,
  kProjectDisallowed = 4,
  kLogSizeExceeded = 5,
  kMaxValue = kLogSizeExceeded,
};

inline constexpr std::string_view kExternalMetricsProducedHistogramPrefix =
    "StructuredMetrics.ExternalMetricsProduced2.";

inline constexpr std::string_view kExternalMetricsDroppedHistogramPrefix =
    "StructuredMetrics.ExternalMetricsDropped2.";

void LogEventRecordingState(EventRecordingState state);

// Log how many structured metrics events were contained in a call to
// ProvideCurrentSessionData.
void LogNumEventsInUpload(int num_events);

// Logs the number of events that were recorded before device and user
// cryptographic keys have been loaded to hash events. These events will be kept
// in memory.
void LogNumEventsRecordedBeforeInit(int num_events);

// Logs the number of files processed per external metrics scan.
void LogNumFilesPerExternalMetricsScan(int num_files);

// Logs the file size of an event.
void LogEventFileSizeKB(int64_t file_size_kb);

// Logs the serialized size of an event when it is recorded in bytes.
void LogEventSerializedSizeBytes(int64_t event_size_bytes);

// Logs the StructuredMetrics uploaded size to UMA in bytes.
void LogUploadSizeBytes(int64_t upload_size_bytes);

// Logs the number of external metrics were scanned for an upload.
void LogExternalMetricsScanInUpload(int num_scans);

// Logs the number of external metrics that were dropped.
void LogDroppedExternalMetrics(int num_dropped);

// Logs the number of external metrics that were dropped per-project.
void LogDroppedProjectExternalMetrics(std::string_view project_name,
                                      int num_dropped);

// Logs the number of external metrics produced per-project.
void LogProducedProjectExternalMetrics(std::string_view project_name,
                                       int num_produced);

// Possible status of the Storage Manager when flushing a buffer to disk. These
// values must match the values in
// tools/metrics/histograms/metadata/structured_metrics/enums.xml.
enum class StorageManagerFlushStatus {
  kSuccessful = 0,
  kWriteError = 1,
  kDiskFull = 2,
  kEventSerializationError = 3,
  kQuotaExceeded = 4,
  kMaxValue = kQuotaExceeded,
};

// Possible status when an event is recorded to the Storage Manager. These
// values must match the values in
// tools/metrics/histograms/metadata/structured_metrics/enums.xml.
enum class RecordStatus {
  kOk = 0,
  kFlushed = 1,
  kFull = 2,
  kError = 3,
  kMaxValue = kError,
};

// Possible internal errors of the FlushedMap. These should
// be looked at in absolute counts. These values must match the values in
// tools/metrics/histograms/metadata/structured_metrics/enums.xml.
enum class FlushedMapError {
  kDeletedInvalidKey = 0,
  kEventSerializationError = 1,
  kFailedToReadKey = 2,
  kMaxValue = kFailedToReadKey,
};

// Logs Storage Managers result when flushing a buffer.
void LogStorageManagerFlushStatus(StorageManagerFlushStatus status);

// Logs internal errors of the FlushedMap.
void LogFlushedMapError(FlushedMapError error);

// Logs the number of FlushedKeys that are loaded at boot.
void LogFlushedMapLoadedFlushedKeys(int count);

// Logs the number of flushed buffers that were deleted when disk quota is
// reached.
void LogDeletedBuffersWhenOverQuota(int count);

// Logs the number of bytes the disk quota has been exceeded. This should be
// proportional to the number of buffers deleted.
void LogDiskQuotaExceededDelta(int delta_kb);

// Logs the number of flushed buffers when an upload occurs.
//
// With the current implementation, this is implying that this is the number of
// buffers read when creating the uploaded log.
void LogFlushedBuffersAtUpload(int count);

// Logs the number of in-memory events when an upload occurs.
void LogInMemoryEventsAtUpload(int count);

// Logs the max disk size in kb that the Storage Manager can consume.
void LogMaxDiskSizeKb(int size_kb);

// Logs the max amount of memory in kb that the in-memory events can consume.
void LogMaxMemorySizeKb(int size_kb);

// Logs the status of recording an event.
void LogStorageManagerRecordStatus(RecordStatus status);

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_HISTOGRAM_UTIL_H_
