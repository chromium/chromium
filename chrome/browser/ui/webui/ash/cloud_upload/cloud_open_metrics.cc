// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_open_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"

namespace ash::cloud_upload {

CloudOpenMetrics::CloudOpenMetrics(CloudProvider cloud_provider)
    : cloud_provider_(cloud_provider),
      task_result_(cloud_provider_ == CloudProvider::kGoogleDrive
                       ? kGoogleDriveTaskResultMetricName
                       : kOneDriveTaskResultMetricName),
      transfer_required_(cloud_provider_ == CloudProvider::kGoogleDrive
                             ? kDriveTransferRequiredMetric
                             : kOneDriveTransferRequiredMetric) {}

CloudOpenMetrics::~CloudOpenMetrics() = default;

void CloudOpenMetrics::LogTaskResult(OfficeTaskResult value) {
  task_result_.Log(value);
}

void CloudOpenMetrics::LogTransferRequired(OfficeFilesTransferRequired value) {
  transfer_required_.Log(value);
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
  base::UmaHistogramEnumeration(metric_name_, value);
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

}  // namespace ash::cloud_upload
