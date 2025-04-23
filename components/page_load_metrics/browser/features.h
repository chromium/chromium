// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_FEATURES_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_FEATURES_H_

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace page_load_metrics::features {

BASE_DECLARE_FEATURE(kSendContinuousInputEventsToObservers);

// The feature flag to enable beacon leakage-related logging logic.
BASE_DECLARE_FEATURE(kBeaconLeakageLogging);

// The param name of the URL category for beacon-leakage-related logging logic.
BASE_DECLARE_FEATURE_PARAM(std::string, kBeaconLeakageLoggingCategoryParamName);

// The prefix of the URL category for beacon-leakage-related logging logic.
BASE_DECLARE_FEATURE_PARAM(std::string, kBeaconLeakageLoggingCategoryPrefix);

// Enables or disables per-frame memory monitoring.
BASE_DECLARE_FEATURE(kV8PerFrameMemoryMonitoring);

// Enables to emit zero values for some key metrics when back-forward cache is
// used.
//
// With this flag disabled, no samples are emitted for regular VOLT metrics
// after the page is restored from the back-forward cache. This means that we
// will miss a lot of metrics for history navigations after we launch back-
// forward cache. As metrics for history navigations tend to be better figures
// than other navigations (e.g., due to network cache), the average of such
// metrics values will become worse and might seem regression if we don't take
// any actions.
//
// To mitigate this issue, we plan to emit 0 samples for such key metrics for
// back-forward navigations. This is implemented behind this flag so far, and we
// will enable this by default when we reach the conclusion how to adjust them.
//
// For cumulative layout shift scores, we use actual score values for back-
// forward cache navigations instead of 0s.
BASE_DECLARE_FEATURE(kBackForwardCacheEmitZeroSamplesForKeyMetrics);

BASE_DECLARE_FEATURE(kClickInputTracker);

}  // namespace page_load_metrics::features

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_FEATURES_H_
