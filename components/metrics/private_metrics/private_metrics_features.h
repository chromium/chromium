// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_METRICS_FEATURES_H_
#define COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_METRICS_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace metrics::private_metrics {

// Enables Private Metrics reporting. This flag enables the flow for reporting
// `PrivateMetricReport` protocol buffer as described in
// go/chrome-trusted-private-metrics and go/etld-plus-one-metrics.
BASE_DECLARE_FEATURE(kPrivateMetricsFeature);

}  // namespace metrics::private_metrics

#endif  // COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_METRICS_FEATURES_H_
