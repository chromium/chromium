// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_OPEN_METRICS_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_OPEN_METRICS_H_

#include "base/memory/safe_ref.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"

namespace ash::cloud_upload {

enum class MetricState {
  // Not logged and it shouldn’t have been.
  kCorrectlyNotLogged = 0,
  // Logged when it should have been.
  kCorrectlyLogged = 1,
  // Not logged when it should have been.
  kIncorrectlyNotLogged = 2,
  // Logged when it shouldn’t have been.
  kIncorrectlyLogged = 3,
  // Logged more than once.
  kIncorrectlyLoggedMultipleTimes = 4,
  // An unexpected value was logged.
  kWrongValueLogged = 5,
  kMaxValue = kWrongValueLogged,
};

// Represents a metric identified by `metric_name` that logs value of type
// `MetricType`. Log the metric through this class. Keeps track of the value
// logged and the `MetricState` of the metric.
template <typename MetricType>
class Metric {
  static_assert(std::is_same_v<std::underlying_type_t<MetricType>, int>,
                "The underlying type of the MetricType must be an int");

 public:
  explicit Metric(std::string metric_name_to_set)
      : metric_name(metric_name_to_set) {}
  ~Metric() = default;

  // Logs a `new_value` to the metric with `metric_name` and saves it to
  // `value`. Update the state:
  //   kCorrectlyNotLogged  -> kCorrectlyLogged
  //   !kCorrectlyNotLogged -> kIncorrectlyLoggedMultipleTimes
  // Return false if there was a metric inconsistency. That is, if the latter
  // state change occurred.
  bool Log(MetricType new_value) {
    LogMetric(new_value);
    if (state == MetricState::kCorrectlyNotLogged) {
      state = MetricState::kCorrectlyLogged;
    } else {
      state = MetricState::kIncorrectlyLoggedMultipleTimes;
    }
    old_value = value;
    value = new_value;
    return state == MetricState::kCorrectlyLogged;
  }

  // Return true if the `state` is a logged state.
  bool logged() {
    switch (state) {
      case MetricState::kCorrectlyNotLogged:
      case MetricState::kIncorrectlyNotLogged:
        return false;
      case MetricState::kCorrectlyLogged:
      case MetricState::kIncorrectlyLogged:
      case MetricState::kIncorrectlyLoggedMultipleTimes:
      case MetricState::kWrongValueLogged:
        return true;
    }
  }

  // Check metric is not logged, otherwise mark the metric as inconsistent and
  // return false.
  bool IsNotLogged() {
    if (logged()) {
      state = MetricState::kIncorrectlyLogged;
      return false;
    }
    return true;
  }

  // Check metric is logged, otherwise mark the metric as inconsistent and
  // return false.
  bool IsLogged() {
    if (!logged()) {
      state = MetricState::kIncorrectlyNotLogged;
      return false;
    }
    return true;
  }

  void set_state(MetricState new_state) { state = new_state; }

  void set_metric_name(std::string new_metric_name) {
    metric_name = new_metric_name;
  }

  std::string metric_name;
  MetricState state = MetricState::kCorrectlyNotLogged;
  MetricType value;
  MetricType old_value;

 private:
  void LogMetric(MetricType new_value);
};

// Specialise for base::File::Error.
template <>
inline void Metric<base::File::Error>::LogMetric(base::File::Error new_value) {
  base::UmaHistogramExactLinear(metric_name, -new_value,
                                -base::File::FILE_ERROR_MAX);
}

template <class MetricType>
inline void Metric<MetricType>::LogMetric(MetricType new_value) {
  base::UmaHistogramEnumeration(metric_name, new_value);
}

// TODO(b/300861997): Add "LogMetric" functions so metrics can be logged through
// this class. Add ability to track the state of each relevant metric in the
// flow and detect inconsistencies.
// Passed through the cloud upload and open flow. Accessed as a `unique_ptr` or
// a SafeRef.
class CloudOpenMetrics {
 public:
  explicit CloudOpenMetrics(CloudProvider cloud_provider, size_t file_count);
  ~CloudOpenMetrics();

  // Not copyable. Create a SafeRef instead.
  CloudOpenMetrics(const CloudOpenMetrics&) = delete;
  CloudOpenMetrics& operator=(const CloudOpenMetrics&) = delete;

  // Not movable. Move the `unique_ptr` owning `CloudOpenMetrics` instead.
  CloudOpenMetrics(const CloudOpenMetrics&&) = delete;
  CloudOpenMetrics& operator=(CloudOpenMetrics&&) = delete;

  // Log the `value` for the CopyError metric.
  void LogCopyError(base::File::Error value);

  // Log the `value` for the MoveError metric.
  void LogMoveError(base::File::Error value);

  // Log the `value` for the DriveOpenError metric.
  void LogGoogleDriveOpenError(OfficeDriveOpenErrors value);

  // Log the `value` for the OneDriveOpenError metric.
  void LogOneDriveOpenError(OfficeOneDriveOpenErrors value);

  // Log the `value` for the SourceVolume metric.
  void LogSourceVolume(OfficeFilesSourceVolume value);

  // Log the `value` for the TaskResult metric.
  void LogTaskResult(OfficeTaskResult value);

  // Log the `value` for the TransferRequired metric.
  void LogTransferRequired(OfficeFilesTransferRequired value);

  // Log the `value` for the UploadResult metric.
  void LogUploadResult(OfficeFilesUploadResult value);

  // Updates the cloud provider for the cloud upload flow. Updates the metric
  // names to log with.
  void UpdateCloudProvider(CloudProvider cloud_provider);

  base::SafeRef<CloudOpenMetrics> GetSafeRef() const;

  // For testing.
  base::WeakPtr<CloudOpenMetrics> GetWeakPtr();

 private:
  // Print debug information about the detected inconsistency and every metric.
  // If `immediately_dump`, `DumpWithoutCrashing()`, otherwise set
  // `delayed_dump_`.
  template <typename MetricType>
  void OnInconsistencyFound(Metric<MetricType>& metric,
                            bool immediately_dump = true);

  // Expect that the `metric` is not logged. Otherwise update the state and
  // call `OnInconsistencyFound()` with `immediately_dump` as false.
  template <typename MetricType>
  void ExpectNotLogged(Metric<MetricType>& metric);

  // Expect that the `metric` metric is logged with a value. Otherwise update
  // the state and call `OnInconsistencyFound()` with `immediately_dump` as
  // false.
  template <typename MetricType>
  void ExpectLogged(Metric<MetricType>& metric);

  // Update the `metric` state to `kWrongValueLogged` and call
  // `OnInconsistencyFound()` with `immediately_dump` as false.
  template <typename MetricType>
  void SetWrongValueLogged(Metric<MetricType>& metric);

  bool multiple_files_;
  // Whether to `DumpWithoutCrashing()` at the end of the destructor.
  bool delayed_dump_ = false;
  CloudProvider cloud_provider_;
  Metric<base::File::Error> copy_error_;
  Metric<base::File::Error> move_error_;
  Metric<OfficeDriveOpenErrors> drive_open_error_;
  Metric<OfficeOneDriveOpenErrors> one_drive_open_error_;
  Metric<OfficeFilesSourceVolume> source_volume_;
  Metric<OfficeTaskResult> task_result_;
  Metric<OfficeFilesTransferRequired> transfer_required_;
  Metric<OfficeFilesUploadResult> upload_result_;
  base::WeakPtrFactory<CloudOpenMetrics> weak_ptr_factory_{this};
};

}  // namespace ash::cloud_upload

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_OPEN_METRICS_H_
