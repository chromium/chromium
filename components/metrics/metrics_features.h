// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_FEATURES_H_
#define COMPONENTS_METRICS_METRICS_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace metrics::features {
// Determines whether histograms that that are expected to be set on every log
// should be emitted in OnDidCreateMetricsLog() instead of
// ProvideCurrentSessionData().
BASE_DECLARE_FEATURE(kEmitHistogramsEarlier);

// If set, histograms that are expected to be set on every log will be emitted
// in DisableRecording().
extern const base::FeatureParam<bool> kEmitHistogramsForIndependentLogs;

// Determines whether the metrics service should create periodic logs
// asynchronously.
BASE_DECLARE_FEATURE(kMetricsServiceAsyncCollection);
}  // namespace metrics::features

#endif  // COMPONENTS_METRICS_METRICS_FEATURES_H_