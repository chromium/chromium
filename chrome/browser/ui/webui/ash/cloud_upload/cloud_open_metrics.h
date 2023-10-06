// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_OPEN_METRICS_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_OPEN_METRICS_H_

#include "base/memory/safe_ref.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"

namespace ash::cloud_upload {

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

  // Log the `value` for the TaskResult metric.
  void LogTaskResult(OfficeTaskResult value);

  // Log the `value` for the TransferRequired metric.
  void LogTransferRequired(OfficeFilesTransferRequired value);

  base::SafeRef<CloudOpenMetrics> GetSafeRef() const;

  // For testing.
  base::WeakPtr<CloudOpenMetrics> GetWeakPtr();

 private:
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

  // Represents a metric identified by `metric_name_` that logs value of type
  // `MetricType`. Log the metric through this class. Keeps track of the value
  // logged and the `MetricState` of the metric.
  template <typename MetricType>
  class Metric {
   public:
    explicit Metric(std::string metric_name);
    ~Metric() = default;

    // Logs a `value` to the metric with `metric_name_` and saves that `value`.
    // Update the state and log an error for the latter case:
    //   kCorrectlyNotLogged  -> kCorrectlyLogged
    //   !kCorrectlyNotLogged -> kIncorrectlyLoggedMultipleTimes
    void Log(MetricType value);

    MetricState state_ = MetricState::kCorrectlyNotLogged;
    MetricType value_;

   private:
    const std::string metric_name_;
  };

  CloudProvider cloud_provider_;
  Metric<OfficeTaskResult> task_result_;
  Metric<OfficeFilesTransferRequired> transfer_required_;
  base::WeakPtrFactory<CloudOpenMetrics> weak_ptr_factory_{this};
};

}  // namespace ash::cloud_upload

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_OPEN_METRICS_H_
