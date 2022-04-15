// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_METRICS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_METRICS_H_

#include <vector>

namespace data_reduction_proxy {

typedef std::vector<long long> ContentLengthList;

// The number of days of bandwidth usage statistics that are tracked.
const unsigned int kNumDaysInHistory = 60;

// The number of days of bandwidth usage statistics that are presented.
const unsigned int kNumDaysInHistorySummary = 30;

static_assert(kNumDaysInHistorySummary <= kNumDaysInHistory,
              "kNumDaysInHistorySummary should be no larger than "
              "kNumDaysInHistory");

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_METRICS_H_
