// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_FEATURES_H_
#define COMPONENTS_METRICS_METRICS_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"

namespace metrics::features {

// This can be used to disable structured metrics as a whole.
BASE_DECLARE_FEATURE(kStructuredMetrics);

// Determines whether to schedule a flush of persistent histogram memory
// immediately after writing a system profile to it.
BASE_DECLARE_FEATURE(kFlushPersistentSystemProfileOnWrite);

// Determines whether to always flush Local State immediately after an UMA/UKM
// log upload. If this is disabled, Local State is only immediately flushed
// after an upload if this is a mobile platform and the browser is in the
// background.
BASE_DECLARE_FEATURE(kReportingServiceAlwaysFlush);

// Controls trimming for metrics logs. This feature allows tuning of the log
// trimming behaviour via serverside parameters. Do not remove. See
// components/metrics/metrics_service_client.cc and
// components/metrics/unsent_log_store.cc.
// Note: On Android WebView, while this feature still controls whether trimming
// is enabled, a separate feature controls the trimming parameters themselves,
// as they have different defaults than Android Chrome.
// See: android_webview/browser/metrics/aw_metrics_service_client.cc
BASE_DECLARE_FEATURE(kMetricsLogTrimming);

// Creates the ProfileMetricsService, which can be used to log per-profile UMA
// histograms.
// Enabled by default - intended as a kill-switch.
BASE_DECLARE_FEATURE(kPerProfileMetrics);

// Consolidates the application locale logic in MetricsServiceClient.
BASE_DECLARE_FEATURE(kConsolidateMetricsServiceLocales);

// Restructures the metrics privacy settings into a three-state model [kNone,
// kBasic, kAdvanced].
BASE_DECLARE_FEATURE(kRestructureMetricsConsentSettings);

}  // namespace metrics::features

#endif  // COMPONENTS_METRICS_METRICS_FEATURES_H_
