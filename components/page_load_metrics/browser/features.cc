// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/features.h"

#include "base/feature_list.h"

namespace page_load_metrics::features {

// Whether to send continuous events - kTouchMove, kGestureScrollUpdate,
// kGesturePinchUpdate, to page load tracker observers.
BASE_FEATURE(kSendContinuousInputEventsToObservers,
             "SendContinuousInputEventsToObservers",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kBeaconLeakageLogging,
             "BeaconLeakageLogging",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

BASE_FEATURE(kV8PerFrameMemoryMonitoring,
             "V8PerFrameMemoryMonitoring",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kBackForwardCacheEmitZeroSamplesForKeyMetrics,
             "BackForwardCacheEmitZeroSamplesForKeyMetrics",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kClickInputTracker,
             "ClickInputTracker",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace page_load_metrics::features
