// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_features.h"

namespace metrics::features {

BASE_FEATURE(kStructuredMetrics,
             "EnableStructuredMetrics",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFlushPersistentSystemProfileOnWrite,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kReportingServiceAlwaysFlush, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMetricsLogTrimming, base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace metrics::features
