// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_FEATURES_H_
#define COMPONENTS_METRICS_METRICS_FEATURES_H_

#include "base/feature_list.h"

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

}  // namespace metrics::features

#endif  // COMPONENTS_METRICS_METRICS_FEATURES_H_
