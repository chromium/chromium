// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_METRICS_FEATURES_H_
#define COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_METRICS_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace metrics::private_metrics {

// Enables Private Metrics reporting. This flag enables the flow for reporting
// `PrivateMetricReport` protocol buffer as described in
// go/chrome-trusted-private-metrics and go/etld-plus-one-metrics.
COMPONENT_EXPORT(PRIVATE_METRICS_FEATURES)
BASE_DECLARE_FEATURE(kPrivateMetricsFeature);

// Enables Private UMA service. When enabled, PUMA histograms will be reported
// to the Private Metrics endpoint.
//
// Note: it's likely this is not the only feature you want to enable, as a
// specific PUMA type can also be implemented behind a feature.
COMPONENT_EXPORT(PRIVATE_METRICS_FEATURES)
BASE_DECLARE_FEATURE(kPrivateMetricsPuma);

// The following feature params are used to parameterize unsent log store
// limits for PUMA. See UnsentLogStoreLimits.
COMPONENT_EXPORT(PRIVATE_METRICS_FEATURES)
extern const base::FeatureParam<size_t> kPrivateMetricsPumaMinLogQueueCount;
COMPONENT_EXPORT(PRIVATE_METRICS_FEATURES)
extern const base::FeatureParam<size_t> kPrivateMetricsPumaMinLogQueueSizeBytes;
COMPONENT_EXPORT(PRIVATE_METRICS_FEATURES)
extern const base::FeatureParam<size_t> kPrivateMetricsPumaMaxLogSizeBytes;

// Enables Private UMA for Regional Capabilities. Enabling this feature will
// collect and uploads logs of this type of PUMA.
COMPONENT_EXPORT(PRIVATE_METRICS_FEATURES)
BASE_DECLARE_FEATURE(kPrivateMetricsPumaRc);

}  // namespace metrics::private_metrics

#endif  // COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_METRICS_FEATURES_H_
