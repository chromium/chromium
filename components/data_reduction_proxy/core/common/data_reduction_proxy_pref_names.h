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
extern const char kDataUsageReportingEnabled[];
extern const char kDataReductionProxyWasEnabledBefore[];
extern const char kDataReductionProxyLastEnabledTime[];
extern const char kHttpOriginalContentLength[];
extern const char kHttpReceivedContentLength[];

extern const char kThisWeekNumber[];
extern const char kThisWeekServicesDownstreamBackgroundKB[];
extern const char kThisWeekServicesDownstreamForegroundKB[];
extern const char kLastWeekServicesDownstreamBackgroundKB[];
extern const char kLastWeekServicesDownstreamForegroundKB[];
extern const char kThisWeekUserTrafficContentTypeDownstreamKB[];
extern const char kLastWeekUserTrafficContentTypeDownstreamKB[];

// The following are obsolete and are being readded temporarily to delete them
// so that they stop taking up space in profile directories.
//
// TODO(crbug.com/1153394): Remove these again once M90 branches (i.e. so that
// they are removed in M91, after having been live for at least two milestones).
//
// BEGIN OBSOLETE PREFS READDED FOR DELETION
extern const char kDailyContentLengthHttpsWithDataReductionProxyEnabled[];
extern const char kDailyContentLengthLongBypassWithDataReductionProxyEnabled[];
extern const char kDailyContentLengthShortBypassWithDataReductionProxyEnabled[];
extern const char kDailyContentLengthUnknownWithDataReductionProxyEnabled[];
extern const char kDailyContentLengthViaDataReductionProxy[];
extern const char kDailyContentLengthViaDataReductionProxyApplication[];
extern const char kDailyContentLengthViaDataReductionProxyVideo[];
extern const char kDailyContentLengthViaDataReductionProxyUnknown[];
extern const char kDailyContentLengthWithDataReductionProxyEnabled[];
extern const char kDailyContentLengthWithDataReductionProxyEnabledApplication[];
extern const char kDailyContentLengthWithDataReductionProxyEnabledVideo[];
extern const char kDailyContentLengthWithDataReductionProxyEnabledUnknown[];
extern const char kDailyHttpOriginalContentLengthApplication[];
extern const char kDailyHttpOriginalContentLengthVideo[];
extern const char kDailyHttpOriginalContentLengthUnknown[];
extern const char kDailyHttpReceivedContentLengthApplication[];
extern const char kDailyHttpReceivedContentLengthVideo[];
extern const char kDailyHttpReceivedContentLengthUnknown[];
extern const char kDailyOriginalContentLengthViaDataReductionProxy[];
extern const char kDailyOriginalContentLengthViaDataReductionProxyApplication[];
extern const char kDailyOriginalContentLengthViaDataReductionProxyVideo[];
extern const char kDailyOriginalContentLengthViaDataReductionProxyUnknown[];
extern const char kDailyOriginalContentLengthWithDataReductionProxyEnabled[];
extern const char
    kDailyOriginalContentLengthWithDataReductionProxyEnabledApplication[];
extern const char
    kDailyOriginalContentLengthWithDataReductionProxyEnabledVideo[];
extern const char
    kDailyOriginalContentLengthWithDataReductionProxyEnabledUnknown[];
// END OBSOLETE PREFS READDED FOR DELETION

}  // namespace prefs
}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_PREF_NAMES_H_
