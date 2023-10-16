// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_open_metrics.h"

#include <string>

#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"

namespace ash::cloud_upload {

// Stringify the `MetricState` enum.
std::ostream& operator<<(std::ostream& os, MetricState metric_state) {
  switch (metric_state) {
    case MetricState::kCorrectlyNotLogged:
      return os << "Correctly not logged";
    case MetricState::kCorrectlyLogged:
      return os << "Correctly logged";
    case MetricState::kIncorrectlyNotLogged:
      return os << "Incorrectly not logged";
    case MetricState::kIncorrectlyLogged:
      return os << "Incorrectly logged";
    case MetricState::kIncorrectlyLoggedMultipleTimes:
      return os << "Incorrectly logged multiple times";
    case MetricState::kWrongValueLogged:
      return os << "Wrong value logged";
  }
}

// Stringify enums (`MetricType`) that are not the `MetricState`.
template <
    typename MetricType,
    class = std::enable_if<std::is_enum<MetricType>::value &&
                           !std::is_same<MetricType, MetricState>::value>::type>
std::ostream& operator<<(std::ostream& os, const MetricType& value) {
  return os << static_cast<std::underlying_type<MetricType>::type>(value);
}

// Print debug information about this metric.
template <typename MetricType>
std::ostream& operator<<(std::ostream& os, const Metric<MetricType>& metric) {
  os << metric.metric_name;
  os << ": ";
  os << metric.state;
  if (metric.state != MetricState::kCorrectlyNotLogged &&
      metric.state != MetricState::kIncorrectlyNotLogged) {
    os << " as ";
    os << metric.value;
  }
  return os;
}

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

// TODO(b/300861997): Dump without crashing if there was an inconsistency.
CloudOpenMetrics::~CloudOpenMetrics() = default;

void CloudOpenMetrics::LogCopyError(base::File::Error value) {
  if (!copy_error_.Log(value)) {
    PrintMetrics();
  }
}

void CloudOpenMetrics::LogMoveError(base::File::Error value) {
  if (!move_error_.Log(value)) {
    PrintMetrics();
  }
}

void CloudOpenMetrics::LogGoogleDriveOpenError(OfficeDriveOpenErrors value) {
  if (!drive_open_error_.Log(value)) {
    PrintMetrics();
  }
}

void CloudOpenMetrics::LogOneDriveOpenError(OfficeOneDriveOpenErrors value) {
  if (!one_drive_open_error_.Log(value)) {
    PrintMetrics();
  }
}

void CloudOpenMetrics::LogSourceVolume(OfficeFilesSourceVolume value) {
  if (!source_volume_.Log(value)) {
    PrintMetrics();
  }
}

void CloudOpenMetrics::LogTaskResult(OfficeTaskResult value) {
  if (!task_result_.Log(value)) {
    PrintMetrics();
  }
}

void CloudOpenMetrics::LogTransferRequired(OfficeFilesTransferRequired value) {
  if (!transfer_required_.Log(value)) {
    PrintMetrics();
  }
}

void CloudOpenMetrics::LogUploadResult(OfficeFilesUploadResult value) {
  if (!upload_result_.Log(value)) {
    PrintMetrics();
  }
}

base::SafeRef<CloudOpenMetrics> CloudOpenMetrics::GetSafeRef() const {
  return weak_ptr_factory_.GetSafeRef();
}

// For testing.
base::WeakPtr<CloudOpenMetrics> CloudOpenMetrics::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void CloudOpenMetrics::PrintMetrics() {
  LOG(WARNING) << "Metrics: " << std::endl
               << task_result_ << std::endl
               << transfer_required_;
}

}  // namespace ash::cloud_upload
