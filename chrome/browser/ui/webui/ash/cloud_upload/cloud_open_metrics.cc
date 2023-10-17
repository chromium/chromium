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
CloudOpenMetrics::~CloudOpenMetrics() {
  bool google_drive = cloud_provider_ == CloudProvider::kGoogleDrive;
  // TODO(cassycc): Add the rest of inconsistency checks.
  ExpectLogged(task_result_);
  if (task_result_.logged() &&
      (task_result_.value == OfficeTaskResult::kFallbackQuickOffice ||
       task_result_.value == OfficeTaskResult::kCancelledAtFallback)) {
    if (google_drive) {
      ExpectLoggedWith(drive_open_error_,
                       {OfficeDriveOpenErrors::kOffline,
                        OfficeDriveOpenErrors::kDriveFsInterface});
    } else {
      ExpectLoggedWith(one_drive_open_error_,
                       {OfficeOneDriveOpenErrors::kOffline});
    }
    ExpectNotLogged(transfer_required_);
    ExpectNotLogged(upload_result_);
  }
}

void CloudOpenMetrics::LogCopyError(base::File::Error value) {
  copy_error_.Log(value);
  PrintDebugInformationIfInconsistent(copy_error_, /*destructor=*/false);
}

void CloudOpenMetrics::LogMoveError(base::File::Error value) {
  move_error_.Log(value);
  PrintDebugInformationIfInconsistent(move_error_, /*destructor=*/false);
}

void CloudOpenMetrics::LogGoogleDriveOpenError(OfficeDriveOpenErrors value) {
  drive_open_error_.Log(value);
  PrintDebugInformationIfInconsistent(drive_open_error_, /*destructor=*/false);
}

void CloudOpenMetrics::LogOneDriveOpenError(OfficeOneDriveOpenErrors value) {
  one_drive_open_error_.Log(value);
  PrintDebugInformationIfInconsistent(one_drive_open_error_,
                                      /*destructor=*/false);
}

void CloudOpenMetrics::LogSourceVolume(OfficeFilesSourceVolume value) {
  source_volume_.Log(value);
  PrintDebugInformationIfInconsistent(source_volume_, /*destructor=*/false);
}

void CloudOpenMetrics::LogTaskResult(OfficeTaskResult value) {
  task_result_.Log(value);
  PrintDebugInformationIfInconsistent(task_result_, /*destructor=*/false);
}

void CloudOpenMetrics::LogTransferRequired(OfficeFilesTransferRequired value) {
  transfer_required_.Log(value);
  PrintDebugInformationIfInconsistent(transfer_required_, /*destructor=*/false);
}

void CloudOpenMetrics::LogUploadResult(OfficeFilesUploadResult value) {
  upload_result_.Log(value);
  PrintDebugInformationIfInconsistent(upload_result_, /*destructor=*/false);
}

base::SafeRef<CloudOpenMetrics> CloudOpenMetrics::GetSafeRef() const {
  return weak_ptr_factory_.GetSafeRef();
}

// For testing.
base::WeakPtr<CloudOpenMetrics> CloudOpenMetrics::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

template <typename MetricType>
void CloudOpenMetrics::PrintDebugInformationIfInconsistent(
    Metric<MetricType>& metric,
    bool destructor) {
  switch (metric.state) {
    case MetricState::kCorrectlyNotLogged:
    case MetricState::kCorrectlyLogged:
      // Consistent state.
      return;
    case MetricState::kIncorrectlyLoggedMultipleTimes:
      // TODO(cassycc): Log old vs new value as this information cannot be
      // derived.
      if (destructor) {
        // This inconsistency is detected during the cloud upload flow and
        // should not be re-detected in the destructor.
        return;
      }
      [[fallthrough]];
    case MetricState::kIncorrectlyNotLogged:
    case MetricState::kIncorrectlyLogged:
    case MetricState::kWrongValueLogged:
      LOG(ERROR) << "Inconsistent metric found: " << metric;
      break;
  }
  PrintMetrics();
}

template <typename MetricType>
void CloudOpenMetrics::ExpectNotLogged(Metric<MetricType>& metric) {
  metric.MakeInconsistentIfLogged();
  PrintDebugInformationIfInconsistent(metric);
}

template <typename MetricType>
void CloudOpenMetrics::ExpectLogged(Metric<MetricType>& metric) {
  metric.MakeInconsistentIfNotLogged();
  PrintDebugInformationIfInconsistent(metric);
}

template <typename MetricType>
void CloudOpenMetrics::ExpectLoggedWith(Metric<MetricType>& metric,
                                        const std::vector<MetricType>& values) {
  metric.MakeInconsistentIfNotLoggedWith(values);
  PrintDebugInformationIfInconsistent(metric);
}

void CloudOpenMetrics::PrintMetrics() {
  LOG(WARNING) << "Metrics: " << std::endl
               << copy_error_ << std::endl
               << move_error_ << std::endl
               << drive_open_error_ << std::endl
               << one_drive_open_error_ << std::endl
               << source_volume_ << std::endl
               << task_result_ << std::endl
               << transfer_required_ << std::endl
               << upload_result_;
}

}  // namespace ash::cloud_upload
