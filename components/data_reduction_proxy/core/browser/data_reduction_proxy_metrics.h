// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_METRICS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_METRICS_H_

#include <vector>

namespace data_reduction_proxy {

typedef std::vector<long long> ContentLengthList;

// A bypass delay more than this is treated as a long delay.
const int kLongBypassDelayInSeconds = 30 * 60;

// The number of days of bandwidth usage statistics that are tracked.
const unsigned int kNumDaysInHistory = 60;

// The number of days of bandwidth usage statistics that are presented.
const unsigned int kNumDaysInHistorySummary = 30;

static_assert(kNumDaysInHistorySummary <= kNumDaysInHistory,
              "kNumDaysInHistorySummary should be no larger than "
              "kNumDaysInHistory");

enum DataReductionProxyRequestType {
  VIA_DATA_REDUCTION_PROXY,  // A request served by the data reduction proxy.
  // Below are reasons why a request is not served by the enabled data reduction
  // proxy. Off-the-record profile data is not counted in all cases.
  HTTPS,         // An https request.
  SHORT_BYPASS,  // The client is bypassed by the proxy for a short time.
  LONG_BYPASS,   // The client is bypassed by the proxy for a long time (due
                 // to country bypass policy, for example).
  UPDATE,        // An update to already counted request data.
  DIRECT_HTTP,   // An http request with a disabled data reduction proxy.
  UNKNOWN_TYPE,  // Any other reason not listed above.
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_METRICS_H_
