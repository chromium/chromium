// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/structured_metrics_features.h"

#include "base/metrics/field_trial_params.h"

namespace metrics::structured {

BASE_FEATURE(kStructuredMetrics,
             "EnableStructuredMetrics",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEventSequenceLogging,
             "EnableEventSequenceLogging",
             base::FEATURE_DISABLED_BY_DEFAULT);

// TODO(b/181724341): Remove this experimental once the feature is rolled out.
BASE_FEATURE(kBluetoothSessionizedMetrics,
             "BluetoothSessionizedMetrics",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFastPairMetrics,
             "FastPairMetrics",
             base::FEATURE_ENABLED_BY_DEFAULT);

constexpr base::FeatureParam<int> kLimitFilesPerScanParam{&kStructuredMetrics,
                                                          "file_limit", 50};
constexpr base::FeatureParam<int> kFileSizeByteLimitParam{
    &kStructuredMetrics, "file_byte_limit", 50000};

constexpr base::FeatureParam<std::string> kDisallowedProjectsParam{
    &kStructuredMetrics, "disabled_projects", ""};

bool IsIndependentMetricsUploadEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kStructuredMetrics, "enable_independent_metrics_upload", true);
}

int GetFileLimitPerScan() {
  return kLimitFilesPerScanParam.Get();
}

int GetFileSizeByteLimit() {
  return kFileSizeByteLimitParam.Get();
}

std::string GetDisabledProjects() {
  return kDisallowedProjectsParam.Get();
}

}  // namespace metrics::structured
