// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/structured_metrics_features.h"

#include "base/feature_list.h"

namespace metrics::structured {

BASE_FEATURE(kEnabledStructuredMetricsService,
             "EnableStructuredMetricsService",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPhoneHubStructuredMetrics,
             "PhoneHubStructuredMetrics",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEventStorageManager,
             "EventStorageManager",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<int> kLimitFilesPerScanParam{
    &features::kStructuredMetrics, "file_limit", 100};
constexpr base::FeatureParam<int> kFileSizeByteLimitParam{
    &features::kStructuredMetrics, "file_byte_limit", 50000};

constexpr base::FeatureParam<std::string> kDisallowedProjectsParam{
    &features::kStructuredMetrics, "disabled_projects", ""};

constexpr base::FeatureParam<int> kMinLogQueueCount{
    &kEnabledStructuredMetricsService, "min_log_queue_count", 10};

constexpr base::FeatureParam<int> kMinLogQueueSizeBytes{
    &kEnabledStructuredMetricsService, "min_log_queue_size_bytes",
    300 * 1024 * 1024  // 300 KiB
};

constexpr base::FeatureParam<int> kMaxLogSizeBytes{
    &kEnabledStructuredMetricsService, "max_log_size_bytes",
    1024 * 1024 * 1024  // 1 MiB
};

constexpr base::FeatureParam<int> kUploadTimeInSeconds{
    &kEnabledStructuredMetricsService, "upload_time_in_seconds",
    10 * 60  // 40 minutes
};

constexpr base::FeatureParam<int> kExternalMetricsCollectionIntervalInSeconds{
    &features::kStructuredMetrics,
    "external_metrics_collection_interval_in_seconds",
    3 * 60  // 10 minutes
};

constexpr base::FeatureParam<int> kStructuredMetricsUploadCadenceMinutes{
    &features::kStructuredMetrics, "sm_upload_cadence_minutes", 45};

constexpr base::FeatureParam<int> kMaxProtoKiBSize{
    &features::kStructuredMetrics, "max_proto_size_kib", 25};

constexpr base::FeatureParam<int> kEventBackupTimeSec{
    &kEventStorageManager, "event_backup_time_s", 3 * 60  // 3 minutes
};

constexpr base::FeatureParam<double> kMaxBufferSizeQuota{
    &features::kStructuredMetrics, "max_buffer_size_quota", 0.0001};

constexpr base::FeatureParam<double> kMaxDiskSizeQuota{
    &features::kStructuredMetrics, "max_disk_size_quota", 0.001};

int GetFileLimitPerScan() {
  return kLimitFilesPerScanParam.Get();
}

int GetFileSizeByteLimit() {
  return kFileSizeByteLimitParam.Get();
}

int GetUploadCadenceMinutes() {
  return kStructuredMetricsUploadCadenceMinutes.Get();
}

int GetProtoKiBLimit() {
  return kMaxProtoKiBSize.Get();
}

std::string GetDisabledProjects() {
  return kDisallowedProjectsParam.Get();
}

int GetUploadInterval() {
  return kUploadTimeInSeconds.Get();
}

base::TimeDelta GetExternalMetricsCollectionInterval() {
  return base::Seconds(kExternalMetricsCollectionIntervalInSeconds.Get());
}

base::TimeDelta GetBackupTimeDelta() {
  return base::Seconds(kEventBackupTimeSec.Get());
}

double GetMaxBufferSizeRatio() {
  return kMaxBufferSizeQuota.Get();
}

double GetMaxDiskSizeRatio() {
  return kMaxDiskSizeQuota.Get();
}

}  // namespace metrics::structured
