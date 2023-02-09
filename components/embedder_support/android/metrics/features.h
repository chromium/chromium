// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_ANDROID_METRICS_FEATURES_H_
#define COMPONENTS_EMBEDDER_SUPPORT_ANDROID_METRICS_FEATURES_H_

#include "base/feature_list.h"

namespace metrics {

// This results in the metric logging being run
// on a separate thread and blocking until the results
// are retrieved.
// When this is disabled, logging is initiated on the
// main thread and a success status is reported to the
// chromium metrics service immediately.
// Currently disabled by default.
BASE_DECLARE_FEATURE(kAndroidMetricsAsyncMetricLogging);

}  // namespace metrics

#endif  // COMPONENTS_EMBEDDER_SUPPORT_ANDROID_METRICS_FEATURES_H_
