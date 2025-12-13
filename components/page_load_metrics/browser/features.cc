// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/features.h"

#include "base/feature_list.h"

namespace page_load_metrics::features {

// Whether to send continuous events - kTouchMove, kGestureScrollUpdate,
// kGesturePinchUpdate, to page load tracker observers.
BASE_FEATURE(kSendContinuousInputEventsToObservers,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kBeaconLeakageLogging, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(std::string,
                   kBeaconLeakageLoggingCategoryParamName,
                   &kBeaconLeakageLogging,
                   "category_param_name",
                   /*default_value=*/"category");

BASE_FEATURE_PARAM(std::string,
                   kBeaconLeakageLoggingCategoryPrefix,
                   &kBeaconLeakageLogging,
                   "category_prefix",
                   /*default_value=*/"");

BASE_FEATURE(kBackForwardCacheEmitZeroSamplesForKeyMetrics,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kClickInputTracker, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPaidContentMetricsObserver, base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace page_load_metrics::features
