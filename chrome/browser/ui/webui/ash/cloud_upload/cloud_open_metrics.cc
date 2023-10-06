// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_open_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"

namespace ash::cloud_upload {

CloudOpenMetrics::CloudOpenMetrics(CloudProvider cloud_provider)
    : cloud_provider_(cloud_provider),
      transfer_required_(Metric<OfficeFilesTransferRequired>(
          cloud_provider_ == CloudProvider::kGoogleDrive
              ? kDriveTransferRequiredMetric
              : kOneDriveTransferRequiredMetric)) {}

CloudOpenMetrics::~CloudOpenMetrics() = default;

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
  value_ = value;
}

}  // namespace ash::cloud_upload
