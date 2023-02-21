// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_features.h"

namespace metrics::features {
BASE_FEATURE(kEmitHistogramsEarlier,
             "EmitHistogramsEarlier",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<bool> kEmitHistogramsForIndependentLogs{
    &kEmitHistogramsEarlier, "emit_for_independent_logs", true};

BASE_FEATURE(kMetricsServiceAsyncCollection,
             "MetricsServiceAsyncCollection",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMetricsServiceAllowEarlyLogClose,
             "MetricsServiceAllowEarlyLogClose",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMetricsClearLogsOnClonedInstall,
             "MetricsClearLogsOnClonedInstall",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
BASE_FEATURE(kReportingServiceFlushPrefsOnUploadInBackground,
             "ReportingServiceFlushPrefsOnUploadInBackground",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
}  // namespace metrics::features