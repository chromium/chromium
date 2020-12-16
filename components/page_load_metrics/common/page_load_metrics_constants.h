// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_COMMON_PAGE_LOAD_METRICS_CONSTANTS_H_
#define COMPONENTS_PAGE_LOAD_METRICS_COMMON_PAGE_LOAD_METRICS_CONSTANTS_H_

#include "base/feature_list.h"

namespace page_load_metrics {

// Amount of time to delay dispatch of metrics. This allows us to batch and send
// fewer cross-process updates, given that cross-process updates can be
// expensive.
const int kBufferTimerDelayMillis = 1000;

const base::Feature kPageLoadMetricsTimerDelayFeature{
    "PageLoadMetricsTimerDelay", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_COMMON_PAGE_LOAD_METRICS_CONSTANTS_H_
