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

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kMergeSubprocessMetricsOnBgAndFg,
             "MergeSubprocessMetricsOnBgAndFg",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kRestoreUmaClientIdIndependentLogs,
             "RestoreUmaClientIdIndependentLogs",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSubprocessMetricsAsync,
             "SubprocessMetricsAsync",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<bool> kPeriodicMergeAsync{&kSubprocessMetricsAsync,
                                                   "PeriodicMergeAsync", false};

const base::FeatureParam<bool> kDeregisterAsync{&kSubprocessMetricsAsync,
                                                "DeregisterAsync", false};

const base::FeatureParam<bool> kDeregisterSequenced{
    &kSubprocessMetricsAsync, "DeregisterSequenced", false};

BASE_FEATURE(kMetricsServiceAsyncIndependentLogs,
             "MetricsServiceAsyncIndependentLogs",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFlushPersistentSystemProfileOnWrite,
             "FlushPersistentSystemProfileOnWrite",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMetricsServiceDeltaSnapshotInBg,
             "MetricsServiceDeltaSnapshotInBg",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace metrics::features
