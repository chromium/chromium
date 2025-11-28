// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/private_metrics/private_metrics_features.h"

namespace metrics::private_metrics {

BASE_FEATURE(kPrivateMetricsFeature, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPrivateMetricsPuma, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<size_t> kPrivateMetricsPumaMinLogQueueCount{
    &kPrivateMetricsPuma, "puma_min_log_queue_count", 10};

const base::FeatureParam<size_t> kPrivateMetricsPumaMinLogQueueSizeBytes{
    &kPrivateMetricsPuma, "puma_min_log_queue_size_bytes",
    300 * 1024};  // 300 KiB

const base::FeatureParam<size_t> kPrivateMetricsPumaMaxLogSizeBytes{
    &kPrivateMetricsPuma, "puma_max_log_size_bytes", 1024 * 1024};  // 1 MiB

BASE_FEATURE(kPrivateMetricsPumaRc, base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace metrics::private_metrics
