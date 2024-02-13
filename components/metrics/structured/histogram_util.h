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

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_HISTOGRAM_UTIL_H_
