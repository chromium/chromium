// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_features.h"

namespace metrics::features {
BASE_FEATURE(kEmitHistogramsEarlier,
             "EmitHistogramsEarlier",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<bool> kEmitHistogramsForIndependentLogs{
    &kEmitHistogramsEarlier, "emit_for_independent_logs", false};

BASE_FEATURE(kMetricsServiceAsyncCollection,
             "MetricsServiceAsyncCollection",
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace metrics::features