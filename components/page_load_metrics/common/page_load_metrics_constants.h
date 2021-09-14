// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_COMMON_PAGE_LOAD_METRICS_CONSTANTS_H_
#define COMPONENTS_PAGE_LOAD_METRICS_COMMON_PAGE_LOAD_METRICS_CONSTANTS_H_

#include "base/feature_list.h"

namespace page_load_metrics {

const base::Feature kPageLoadMetricsTimerDelayFeature{
    "PageLoadMetricsTimerDelay", base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_COMMON_PAGE_LOAD_METRICS_CONSTANTS_H_
