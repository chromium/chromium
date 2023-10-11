// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_open_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"

namespace ash::cloud_upload {

CloudOpenMetrics::CloudOpenMetrics(CloudProvider cloud_provider)
    : cloud_provider_(cloud_provider),
      copy_error_(cloud_provider_ == CloudProvider::kGoogleDrive
                      ? kGoogleDriveCopyErrorMetricName
                      : kOneDriveCopyErrorMetricName),
      move_error_(cloud_provider_ == CloudProvider::kGoogleDrive
                      ? kGoogleDriveMoveErrorMetricName
                      : kOneDriveMoveErrorMetricName),
      drive_open_error_(kDriveErrorMetricName),
      one_drive_open_error_(kOneDriveErrorMetricName),
      source_volume_(cloud_provider_ == CloudProvider::kGoogleDrive
                         ? kDriveOpenSourceVolumeMetric
                         : kOneDriveOpenSourceVolumeMetric),
      task_result_(cloud_provider_ == CloudProvider::kGoogleDrive
                       ? kGoogleDriveTaskResultMetricName
                       : kOneDriveTaskResultMetricName),
      transfer_required_(cloud_provider_ == CloudProvider::kGoogleDrive
                             ? kDriveTransferRequiredMetric
                             : kOneDriveTransferRequiredMetric),
      upload_result_(cloud_provider_ == CloudProvider::kGoogleDrive
                         ? kGoogleDriveUploadResultMetricName
                         : kOneDriveUploadResultMetricName) {}

CloudOpenMetrics::~CloudOpenMetrics() = default;

void CloudOpenMetrics::LogCopyError(base::File::Error value) {
  copy_error_.Log(value);
}

void CloudOpenMetrics::LogMoveError(base::File::Error value) {
  move_error_.Log(value);
}

void CloudOpenMetrics::LogGoogleDriveOpenError(OfficeDriveOpenErrors value) {
  drive_open_error_.Log(value);
}

void CloudOpenMetrics::LogOneDriveOpenError(OfficeOneDriveOpenErrors value) {
  one_drive_open_error_.Log(value);
}

void CloudOpenMetrics::LogSourceVolume(OfficeFilesSourceVolume value) {
  source_volume_.Log(value);
}

void CloudOpenMetrics::LogTaskResult(OfficeTaskResult value) {
  task_result_.Log(value);
}

void CloudOpenMetrics::LogTransferRequired(OfficeFilesTransferRequired value) {
  transfer_required_.Log(value);
}

void CloudOpenMetrics::LogUploadResult(OfficeFilesUploadResult value) {
  upload_result_.Log(value);
}

base::SafeRef<CloudOpenMetrics> CloudOpenMetrics::GetSafeRef() const {
  return weak_ptr_factory_.GetSafeRef();
}

// For testing.
base::WeakPtr<CloudOpenMetrics> CloudOpenMetrics::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

template <class MetricType>
CloudOpenMetrics::Metric<MetricType>::Metric(std::string metric_name)
    : metric_name_(metric_name) {}

template <class MetricType>
void CloudOpenMetrics::Metric<MetricType>::Log(MetricType value) {
  LogMetric(value);
  if (state_ == MetricState::kCorrectlyNotLogged) {
    state_ = MetricState::kCorrectlyLogged;
  } else {
    state_ = MetricState::kIncorrectlyLoggedMultipleTimes;
    LOG(ERROR) << metric_name_ << " being logged with "
               << static_cast<std::underlying_type<MetricType>::type>(value)
               << " when it was already logged with "
               << static_cast<std::underlying_type<MetricType>::type>(value_);
  }
  value_ = value;
}

template <class MetricType>
void CloudOpenMetrics::Metric<MetricType>::LogMetric(MetricType value) {
  base::UmaHistogramEnumeration(metric_name_, value);
}

// Handle a value of type base::File::Error differently.
template <>
void CloudOpenMetrics::Metric<base::File::Error>::LogMetric(
    base::File::Error value) {
  base::UmaHistogramExactLinear(metric_name_, -value,
                                -base::File::FILE_ERROR_MAX);
}

}  // namespace ash::cloud_upload
