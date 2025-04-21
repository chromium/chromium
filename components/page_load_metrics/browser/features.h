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
}  // namespace page_load_metrics::features

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_FEATURES_H_
