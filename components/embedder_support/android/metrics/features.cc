// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/android/metrics/features.h"

#include "base/feature_list.h"

namespace metrics {

BASE_FEATURE(kAndroidMetricsAsyncMetricLogging,
             "AndroidMetricsAsyncMetricLogging",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace metrics
