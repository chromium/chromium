// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_PREF_NAMES_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_PREF_NAMES_H_

namespace data_reduction_proxy {
namespace prefs {

// Alphabetical list of preference names specific to the data_reduction_proxy
// component. Keep alphabetized, and document each in the .cc file.

extern const char kDailyHttpContentLengthLastUpdateDate[];
extern const char kDailyHttpOriginalContentLength[];
extern const char kDailyHttpReceivedContentLength[];

extern const char kDataReductionProxy[];
extern const char kDataSaverEnabled[];
extern const char kDataReductionProxyConfig[];
extern const char kDataUsageReportingEnabled[];
extern const char kDataReductionProxyWasEnabledBefore[];
extern const char kDataReductionProxyLastEnabledTime[];
extern const char kHttpOriginalContentLength[];
extern const char kHttpReceivedContentLength[];
extern const char kDataReductionProxyLastConfigRetrievalTime[];
extern const char kNetworkProperties[];

extern const char kThisWeekNumber[];
extern const char kThisWeekServicesDownstreamBackgroundKB[];
extern const char kThisWeekServicesDownstreamForegroundKB[];
extern const char kLastWeekServicesDownstreamBackgroundKB[];
extern const char kLastWeekServicesDownstreamForegroundKB[];
extern const char kThisWeekUserTrafficContentTypeDownstreamKB[];
extern const char kLastWeekUserTrafficContentTypeDownstreamKB[];

}  // namespace prefs
}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_PREF_NAMES_H_
