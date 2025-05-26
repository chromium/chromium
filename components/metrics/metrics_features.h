// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_FEATURES_H_
#define COMPONENTS_METRICS_METRICS_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
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
BASE_DECLARE_FEATURE(kMetricsLogTrimming);

#if BUILDFLAG(IS_ANDROID)
// If enabled, when foregrounding, the ReportingService backoff will be reset so
// that uploads are scheduled normally again.
// Context: In crbug.com/420459511, it was discovered that starting from Android
// 15, apps cannot issue network requests from the background (they will fail).
// This resulted in the ReportingService being throttled due to the backoff
// logic, with uploads being scheduled very far ahead in the future (up to 24h).
// During this time, periodic ongoing logs stop getting created. Even if there
// are other scenarios where logs get created (upon backgrounding and
// foregrounding), those simply accumulate on disk since no logs are being
// uploaded. This can eventually lead to log trimming which in turn leads to
// data loss.
// TODO: crbug.com/420459511 - This feature mitigates data loss, but doesn't fix
// the issue that periodic ongoing logs stop being created while in the
// background (which is supposed to be controlled by the `UMABackgroundSessions`
// feature). Integrate with WorkManager, JobScheduler, and/or
// `background_task::BackgroundTask` for a proper fix of background metrics.
BASE_DECLARE_FEATURE(kResetMetricsUploadBackoffOnForeground);
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace metrics::features

#endif  // COMPONENTS_METRICS_METRICS_FEATURES_H_
