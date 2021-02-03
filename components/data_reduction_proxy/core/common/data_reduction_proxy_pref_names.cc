// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"

namespace data_reduction_proxy {
namespace prefs {

// An int64_t pref that contains an internal representation of midnight on the
// date of the last update to |kDailyHttp{Original,Received}ContentLength|.
const char kDailyHttpContentLengthLastUpdateDate[] =
    "data_reduction.last_update_date";

// A List pref that contains daily totals of the original size of all HTTP/HTTPS
// content received from the network.
const char kDailyHttpOriginalContentLength[] =
    "data_reduction.daily_original_length";

// A List pref that contains daily totals of the size of all HTTP/HTTPS content
// received from the network.
const char kDailyHttpReceivedContentLength[] =
    "data_reduction.daily_received_length";

// String that specifies the origin allowed to use data reduction proxy
// authentication, if any.
const char kDataReductionProxy[] = "auth.spdyproxy.origin";

// A boolean specifying whether the DataSaver feature is enabled for this
// client. Note that this preference key name is a legacy string for the sdpy
// proxy.
//
// WARNING: This pref is not the source of truth for determining if Data Saver
// is enabled. Use |DataReductionSettings::IsDataSaverEnabledByUser| instead or
// consult the OWNERS.
const char kDataSaverEnabled[] = "spdy_proxy.enabled";

// A boolean specifying whether data usage should be collected for reporting.
const char kDataUsageReportingEnabled[] = "data_usage_reporting.enabled";

// A boolean specifying whether the data reduction proxy was ever enabled
// before.
const char kDataReductionProxyWasEnabledBefore[] =
    "spdy_proxy.was_enabled_before";

// An integer pref that contains the time when the data reduction proxy was last
// enabled. Recorded only if the data reduction proxy was last enabled since
// this pref was added.
const char kDataReductionProxyLastEnabledTime[] =
    "data_reduction.last_enabled_time";

// An int64_t pref that contains the total size of all HTTP content received
// from the network.
const char kHttpReceivedContentLength[] = "http_received_content_length";

// An int64_t pref that contains the total original size of all HTTP content
// received over the network.
const char kHttpOriginalContentLength[] = "http_original_content_length";

// An integer pref that stores the number of the week when the weekly data use
// prefs were updated.
const char kThisWeekNumber[] = "data_reduction.this_week_number";

// Dictionary pref that stores the data use of services. The key will be the
// service hash code, and the value will be the KB that service used.
const char kThisWeekServicesDownstreamBackgroundKB[] =
    "data_reduction.this_week_services_downstream_background_kb";
const char kThisWeekServicesDownstreamForegroundKB[] =
    "data_reduction.this_week_services_downstream_foreground_kb";
const char kLastWeekServicesDownstreamBackgroundKB[] =
    "data_reduction.last_week_services_downstream_background_kb";
const char kLastWeekServicesDownstreamForegroundKB[] =
    "data_reduction.last_week_services_downstream_foreground_kb";

// Dictionary pref that stores the content-type of user-initiated traffic. The
// key will be the content-type, and the value will be the data usage in KB.
const char kThisWeekUserTrafficContentTypeDownstreamKB[] =
    "data_reduction.this_week_user_traffic_contenttype_downstream_kb";
const char kLastWeekUserTrafficContentTypeDownstreamKB[] =
    "data_reduction.last_week_user_traffic_contenttype_downstream_kb";

// The following prefs are obsolete and are being readded temporarily to clear
// them.

// BEGIN OBSOLETE PREFS READDED FOR DELETION

// A List pref that contains daily totals of the size of all HTTPS
// content received when the data reduction proxy was enabled.
const char kDailyContentLengthHttpsWithDataReductionProxyEnabled[] =
    "data_reduction.daily_received_length_https_with_"
    "data_reduction_proxy_enabled";

// A List pref that contains daily totals of the size of all HTTP/HTTPS
// content received when a bypass of more than 30 minutes is in effect.
const char kDailyContentLengthLongBypassWithDataReductionProxyEnabled[] =
    "data_reduction.daily_received_length_long_bypass_with_"
    "data_reduction_proxy_enabled";

// A List pref that contains daily totals of the size of all HTTP/HTTPS
// content received when a bypass of less than 30 minutes is in effect.
const char kDailyContentLengthShortBypassWithDataReductionProxyEnabled[] =
    "data_reduction.daily_received_length_short_bypass_with_"
    "data_reduction_proxy_enabled";

// TODO(bengr): what is this?
const char kDailyContentLengthUnknownWithDataReductionProxyEnabled[] =
    "data_reduction.daily_received_length_unknown_with_"
    "data_reduction_proxy_enabled";

// A List pref that contains daily totals of the size of all HTTP/HTTPS
// content received via the data reduction proxy.
const char kDailyContentLengthViaDataReductionProxy[] =
    "data_reduction.daily_received_length_via_data_reduction_proxy";

// A List pref that contains daily totals of the size of all HTTP/HTTPS
// content with application mime-type received via the data reduction proxy.
const char kDailyContentLengthViaDataReductionProxyApplication[] =
    "data_reduction.daily_received_length_via_data_reduction_proxy_application";

// A List pref that contains daily totals of the size of all HTTP/HTTPS
// content with video mime-type received via the data reduction proxy.
const char kDailyContentLengthViaDataReductionProxyVideo[] =
    "data_reduction.daily_received_length_via_data_reduction_proxy_video";

// A List pref that contains daily totals of the size of all HTTP/HTTPS
// content with unknown mime-type received via the data reduction proxy.
const char kDailyContentLengthViaDataReductionProxyUnknown[] =
    "data_reduction.daily_received_length_via_data_reduction_proxy_unknown";

// A List pref that contains daily totals of the size of all HTTP/HTTPS
// content received while the data reduction proxy is enabled.
const char kDailyContentLengthWithDataReductionProxyEnabled[] =
    "data_reduction.daily_received_length_with_data_reduction_proxy_enabled";

// A List pref that contains daily totals of the size of all HTTP/HTTPS
// content with application mime-type received while the data reduction proxy is
// enabled.
const char kDailyContentLengthWithDataReductionProxyEnabledApplication[] =
    "data_reduction.daily_received_length_with_data_reduction_proxy_enabled_"
    "application";

// A List pref that contains daily totals of the size of all HTTP/HTTPS
// content with video mime-type received while the data reduction proxy is
// enabled.
const char kDailyContentLengthWithDataReductionProxyEnabledVideo[] =
    "data_reduction.daily_received_length_with_data_reduction_proxy_enabled_"
    "video";

// A List pref that contains daily totals of the size of all HTTP/HTTPS
// content with unknown mime-type received while the data reduction proxy is
// enabled.
const char kDailyContentLengthWithDataReductionProxyEnabledUnknown[] =
    "data_reduction.daily_received_length_with_data_reduction_proxy_enabled_"
    "unknown";

// A List pref that contains daily totals of the original size of all HTTP/HTTPS
// content with application mime-type received from the network.
const char kDailyHttpOriginalContentLengthApplication[] =
    "data_reduction.daily_original_length_application";

// A List pref that contains daily totals of the original size of all HTTP/HTTPS
// content with video mime-type received from the network.
const char kDailyHttpOriginalContentLengthVideo[] =
    "data_reduction.daily_original_length_video";

// A List pref that contains daily totals of the original size of all HTTP/HTTPS
// content with unknown mime-type received from the network.
const char kDailyHttpOriginalContentLengthUnknown[] =
    "data_reduction.daily_original_length_unknown";

// A List pref that contains daily totals of the size of all HTTP/HTTPS content
// received with application mime-type  from the network.
const char kDailyHttpReceivedContentLengthApplication[] =
    "data_reduction.daily_received_length_application";

// A List pref that contains daily totals of the size of all HTTP/HTTPS content
// received with video mime-type from the network.
const char kDailyHttpReceivedContentLengthVideo[] =
    "data_reduction.daily_received_length_video";

// A List pref that contains daily totals of the size of all HTTP/HTTPS content
// received with unknown mime-type  from the network.
const char kDailyHttpReceivedContentLengthUnknown[] =
    "data_reduction.daily_received_length_unknown";

// A List pref that contains daily totals of the original size of all HTTP/HTTPS
// content received via the data reduction proxy.
const char kDailyOriginalContentLengthViaDataReductionProxy[] =
    "data_reduction.daily_original_length_via_data_reduction_proxy";

// A List pref that contains daily totals of the original size of all HTTP/HTTPS
// content with application mime-type received via the data reduction proxy.
const char kDailyOriginalContentLengthViaDataReductionProxyApplication[] =
    "data_reduction.daily_original_length_via_data_reduction_proxy_application";

// A List pref that contains daily totals of the original size of all HTTP/HTTPS
// content with video mime-type received via the data reduction proxy.
const char kDailyOriginalContentLengthViaDataReductionProxyVideo[] =
    "data_reduction.daily_original_length_via_data_reduction_proxy_video";

// A List pref that contains daily totals of the original size of all HTTP/HTTPS
// content with unknown mime-type received via the data reduction proxy.
const char kDailyOriginalContentLengthViaDataReductionProxyUnknown[] =
    "data_reduction.daily_original_length_via_data_reduction_proxy_unknown";

// A List pref that contains daily totals of the original size of all HTTP/HTTPS
// content received while the data reduction proxy is enabled.
const char kDailyOriginalContentLengthWithDataReductionProxyEnabled[] =
    "data_reduction.daily_original_length_with_data_reduction_proxy_enabled";

// A List pref that contains daily totals of the original size of all HTTP/HTTPS
// content with application mime type received while the data reduction proxy is
// enabled.
const char
    kDailyOriginalContentLengthWithDataReductionProxyEnabledApplication[] =
        "data_reduction.daily_original_length_with_data_reduction_proxy_"
        "enabled_application";

// A List pref that contains daily totals of the original size of all HTTP/HTTPS
// content with video mime type received while the data reduction proxy is
// enabled.
const char kDailyOriginalContentLengthWithDataReductionProxyEnabledVideo[] =
    "data_reduction.daily_original_length_with_data_reduction_proxy_enabled_"
    "video";

// A List pref that contains daily totals of the original size of all HTTP/HTTPS
// content with unknown mime type received while the data reduction proxy is
// enabled.
const char kDailyOriginalContentLengthWithDataReductionProxyEnabledUnknown[] =
    "data_reduction.daily_original_length_with_data_reduction_proxy_enabled_"
    "unknown";

// END OBSOLETE PREFS READDED FOR DELETION

}  // namespace prefs
}  // namespace data_reduction_proxy
