// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_features.h"

namespace metrics::features {

BASE_FEATURE(kMetricsServiceAllowEarlyLogClose,
             "MetricsServiceAllowEarlyLogClose",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kStructuredMetrics,
             "EnableStructuredMetrics",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFlushPersistentSystemProfileOnWrite,
             "FlushPersistentSystemProfileOnWrite",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMetricsServiceDeltaSnapshotInBg,
             "MetricsServiceDeltaSnapshotInBg",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kReportingServiceAlwaysFlush,
             "ReportingServiceAlwaysFlush",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMetricsLogTrimming,
             "MetricsLogTrimming",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace metrics::features
