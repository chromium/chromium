// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_UTIL_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_UTIL_H_

#include <memory>
#include <string>

#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "net/base/network_change_notifier.h"
#include "net/base/proxy_server.h"
#include "net/nqe/effective_connection_type.h"
#include "net/proxy_resolution/proxy_retry_info.h"
#include "url/gurl.h"

namespace base {
class Time;
class TimeDelta;
}  // namespace base

namespace data_reduction_proxy {

enum class Client {
  UNKNOWN,
  CRONET_ANDROID,
  WEBVIEW_ANDROID,
  CHROME_ANDROID,
  CHROME_IOS,
  CHROME_MAC,
  CHROME_CHROMEOS,
  CHROME_LINUX,
  CHROME_WINDOWS,
  CHROME_FREEBSD,
  CHROME_OPENBSD,
  CHROME_SOLARIS,
  CHROME_QNX,
};

namespace util {

// Returns the version of Chromium that is being used, e.g. "1.2.3.4".
const char* ChromiumVersion();

// Returns the build and patch numbers of |version_string| as std::string.
// |version_string| must be a properly formed Chromium version number, e.g.
// "1.2.3.4".
void GetChromiumBuildAndPatch(const std::string& version_string,
                              std::string* build,
                              std::string* patch);

// Returns the build and patch numbers of |version_string| as unit32_t.
// |version_string| must be a properly formed Chromium version number, e.g.
// "1.2.3.4".
void GetChromiumBuildAndPatchAsInts(const std::string& version_string,
                                    uint32_t* build,
                                    uint32_t* patch);

// Get the human-readable version of |client|.
const char* GetStringForClient(Client client);

GURL AddApiKeyToUrl(const GURL& url);

// Returns the hostname used for the other bucket to record datause not scoped
// to a page load such as chrome-services traffic, service worker, Downloads.
const char* GetSiteBreakdownOtherHostName();

}  // namespace util

namespace protobuf_parser {

static_assert(net::EFFECTIVE_CONNECTION_TYPE_LAST == 6,
              "If net::EFFECTIVE_CONNECTION_TYPE changes, "
              "PageloadMetrics_EffectiveConnectionType needs to be updated.");

// Returns the |net::ProxyServer::Scheme| for a ProxyServer_ProxyScheme.
net::ProxyServer::Scheme SchemeFromProxyScheme(
    ProxyServer_ProxyScheme proxy_scheme);

// Returns the ProxyServer_ProxyScheme for a |net::ProxyServer::Scheme|.
ProxyServer_ProxyScheme ProxySchemeFromScheme(net::ProxyServer::Scheme scheme);

// Returns the |Duration| representation of |time_delta|.
void TimeDeltaToDuration(const base::TimeDelta& time_delta, Duration* duration);

// Returns the |base::TimeDelta| representation of |duration|.  This is accurate
// to the microsecond.
base::TimeDelta DurationToTimeDelta(const Duration& duration);

// Returns the |Timestamp| representation of |time|.
void TimeToTimestamp(const base::Time& time, Timestamp* timestamp);

// Returns the |Time| representation of |timestamp|. This is accurate to the
// microsecond.
base::Time TimestampToTime(const Timestamp& timestamp);

// Returns an allocated |Duration| unique pointer.
std::unique_ptr<Duration> CreateDurationFromTimeDelta(
    const base::TimeDelta& time_delta);

// Returns an allocated |Timestamp| unique pointer.
std::unique_ptr<Timestamp> CreateTimestampFromTime(const base::Time& time);

}  // namespace protobuf_parser

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_UTIL_H_
