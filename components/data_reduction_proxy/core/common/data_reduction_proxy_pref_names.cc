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

// String that specifies a persisted Data Reduction Proxy configuration.
const char kDataReductionProxyConfig[] = "data_reduction.config";

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

// Pref to store the retrieval time of the last Data Reduction Proxy
// configuration.
const char kDataReductionProxyLastConfigRetrievalTime[] =
    "data_reduction.last_config_retrieval_time";

// Pref to store the properties of the different networks. The pref stores the
// map of network IDs and their respective network properties.
const char kNetworkProperties[] = "data_reduction.network_properties";

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

}  // namespace prefs
}  // namespace data_reduction_proxy
