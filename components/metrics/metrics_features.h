// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_FEATURES_H_
#define COMPONENTS_METRICS_METRICS_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace metrics::features {

// Determines at what point the metrics service is allowed to close a log when
// Chrome is closed (and backgrounded/foregrounded for mobile platforms). When
// this feature is disabled, the metrics service can only close a log if it has
// already started sending logs. When this feature is enabled, the metrics
// service can close a log starting from when the first log is opened.
BASE_DECLARE_FEATURE(kMetricsServiceAllowEarlyLogClose);

// Determines whether logs stored in Local State are cleared when the Chrome
// install is detected as cloned.
BASE_DECLARE_FEATURE(kMetricsClearLogsOnClonedInstall);

// This can be used to disable structured metrics as a whole.
BASE_DECLARE_FEATURE(kStructuredMetrics);

#if BUILDFLAG(IS_ANDROID)
// Determines whether to merge histograms from child processes when Chrome is
// backgrounded/foregrounded. Only on Android.
BASE_DECLARE_FEATURE(kMergeSubprocessMetricsOnBgAndFg);
#endif  // BUILDFLAG(IS_ANDROID)

// When this feature is enabled, use the client ID stored in the system profile
// of the PMA files when creating independent logs from them. This is to address
// the issue of a client resetting their client ID, and then creating an
// independent log from a previous session that used a different client ID.
// Without this feature, this independent log would be using the new client ID,
// although the metrics are associated with the old client ID. This is notably
// the case in cloned installs.
BASE_DECLARE_FEATURE(kRestoreUmaClientIdIndependentLogs);

// Determines whether to allow merging subprocess metrics asynchronously. By
// itself, the feature does nothing. But the different params below allow
// toggling specific async behaviours.
BASE_DECLARE_FEATURE(kSubprocessMetricsAsync);
// Determines whether to merge subprocess metrics asynchronously when creating
// periodic ongoing UMA logs.
extern const base::FeatureParam<bool> kPeriodicMergeAsync;
// Determines whether to merge the last metrics of a subprocess that has just
// exited asynchronously.
extern const base::FeatureParam<bool> kDeregisterAsync;
// Determines whether the tasks posted when deregistering a subprocess
// asynchronously are sequenced. This param only applies when |kDeregisterAsync|
// is true.
extern const base::FeatureParam<bool> kDeregisterSequenced;

// Determines whether the metrics service should finalize certain independent
// logs asynchronously.
BASE_DECLARE_FEATURE(kMetricsServiceAsyncIndependentLogs);

}  // namespace metrics::features

#endif  // COMPONENTS_METRICS_METRICS_FEATURES_H_
