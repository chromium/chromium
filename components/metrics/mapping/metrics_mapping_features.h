// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_MAPPING_METRICS_MAPPING_FEATURES_H_
#define COMPONENTS_METRICS_MAPPING_METRICS_MAPPING_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace metrics::features {

// Controls whether Webium (Initial WebUI) telemetry is separated from
// general web browsing metrics. When disabled, all metrics are reported as-is.
// When enabled, metrics are filtered or renamed according to the provided
// configuration rules.
COMPONENT_EXPORT(METRICS_MAPPING) BASE_DECLARE_FEATURE(kWebiumMetricsMapping);

// The base64-encoded binary proto configuration for the Webium metrics mapping
// feature.
// If this string is empty, all metrics from the Webium renderer process will be
// dropped. Otherwise, the string will be used to map the metric names.
COMPONENT_EXPORT(METRICS_MAPPING)
BASE_DECLARE_FEATURE_PARAM(std::string, kWebiumMetricsMappingConfig);

}  // namespace metrics::features

#endif  // COMPONENTS_METRICS_MAPPING_METRICS_MAPPING_FEATURES_H_
