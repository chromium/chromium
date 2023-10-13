// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_OPEN_METRICS_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_OPEN_METRICS_H_

#include "base/memory/safe_ref.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"

namespace ash::cloud_upload {

enum class MetricState {
  // Not logged and it shouldn’t have been.
  kCorrectlyNotLogged,
  // Logged when it should have been.
  kCorrectlyLogged,
  // Not logged when it should have been.
  kIncorrectlyNotLogged,
  // Logged when it shouldn’t have been.
  kIncorrectlyLogged,
  // Logged more than once.
  kIncorrectlyLoggedMultipleTimes,
  // An unexpected value was logged.
  kWrongValueLogged,
};

// TODO(b/300861997): Add "LogMetric" functions so metrics can be logged through
// this class. Add ability to track the state of each relevant metric in the
// flow and detect inconsistencies.
// Passed through the cloud upload and open flow. Accessed as a `unique_ptr` or
// a SafeRef.
class CloudOpenMetrics {
 public:
  explicit CloudOpenMetrics(CloudProvider cloud_provider);
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

  base::SafeRef<CloudOpenMetrics> GetSafeRef() const;

  // For testing.
  base::WeakPtr<CloudOpenMetrics> GetWeakPtr();

  // Represents a metric identified by `metric_name` that logs value of type
  // `MetricType`. Log the metric through this class. Keeps track of the value
  // logged and the `MetricState` of the metric.
  template <typename MetricType>
  class Metric {
    static_assert(std::is_same_v<std::underlying_type_t<MetricType>, int>,
                  "The underlying type of the MetricType must be an int");

   public:
    explicit Metric(std::string metric_name_to_set);
    ~Metric() = default;

    // Logs a `new_value` to the metric with `metric_name` and saves it to
    // `value`. Update the state:
    //   kCorrectlyNotLogged  -> kCorrectlyLogged
    //   !kCorrectlyNotLogged -> kIncorrectlyLoggedMultipleTimes
    // Return false if there was a metric inconsistency. That is, if the latter
    // state change occurred.
    bool Log(MetricType new_value);

    void set_state(MetricState new_state);

    const std::string metric_name;
    MetricState state = MetricState::kCorrectlyNotLogged;
    MetricType value;

   private:
    void LogMetric(MetricType new_value);
  };

 private:
  // Print the debug information for each metric.
  void PrintMetrics();

  // TODO(b/300861997): Dump without crashing.
  // Handle a metric inconsistency by printing metric information.
  void HandleInconsistency();

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
