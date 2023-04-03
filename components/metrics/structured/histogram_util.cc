// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/histogram_util.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"

namespace metrics::structured {

void LogInternalError(StructuredMetricsError error) {
  UMA_HISTOGRAM_ENUMERATION("UMA.StructuredMetrics.InternalError2", error);
}

void LogEventRecordingState(EventRecordingState state) {
  UMA_HISTOGRAM_ENUMERATION("UMA.StructuredMetrics.EventRecordingState", state);
}

void LogNumEventsInUpload(const int num_events) {
  UMA_HISTOGRAM_COUNTS_1000("UMA.StructuredMetrics.NumEventsInUpload",
                            num_events);
}

void LogKeyValidation(KeyValidationState state) {
  UMA_HISTOGRAM_ENUMERATION("UMA.StructuredMetrics.KeyValidationState", state);
}

void LogIsEventRecordedUsingMojo(bool used_mojo_api) {
  UMA_HISTOGRAM_BOOLEAN("UMA.StructuredMetrics.EventsRecordedUsingMojo",
                        used_mojo_api);
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
  base::UmaHistogramCounts1000("UMA.StructuredMetrics.UploadSize",
                               upload_size_bytes);
}

void LogExternalMetricsScanInUpload(int num_scans) {
  base::UmaHistogramExactLinear(
      "UMA.StructuredMetrics.ExternalMetricScansPerUpload", num_scans, 10);
}

}  // namespace metrics::structured
