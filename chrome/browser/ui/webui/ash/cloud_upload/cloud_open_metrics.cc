// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_open_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"

namespace ash::cloud_upload {

CloudOpenMetrics::CloudOpenMetrics(CloudProvider cloud_provider)
    : cloud_provider_(cloud_provider) {}

CloudOpenMetrics::~CloudOpenMetrics() = default;

void CloudOpenMetrics::LogTransferRequired(OfficeFilesTransferRequired value) {
  std::string metric_name = cloud_provider_ == CloudProvider::kGoogleDrive
                                ? kDriveTransferRequiredMetric
                                : kOneDriveTransferRequiredMetric;
  base::UmaHistogramEnumeration(metric_name, value);
}

base::SafeRef<CloudOpenMetrics> CloudOpenMetrics::GetSafeRef() const {
  return weak_ptr_factory_.GetSafeRef();
}

// For testing.
base::WeakPtr<CloudOpenMetrics> CloudOpenMetrics::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash::cloud_upload
