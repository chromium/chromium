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

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
// Determines whether we immediately flush Local State after uploading a log
// while Chrome is in the background. Only applicable for mobile platforms.
BASE_DECLARE_FEATURE(kReportingServiceFlushPrefsOnUploadInBackground);
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

// Whether SubprocessMetricsProvider should be leaky, so that it can listen to
// subprocesses exiting even after the MetricsService has been destroyed.
BASE_DECLARE_FEATURE(kSubprocessMetricsProviderLeaky);

// This can be used to disable structured metrics as a whole.
BASE_DECLARE_FEATURE(kStructuredMetrics);

}  // namespace metrics::features

#endif  // COMPONENTS_METRICS_METRICS_FEATURES_H_
