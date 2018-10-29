// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_UTIL_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_UTIL_H_

#include <memory>
#include <string>

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_page_load_timing.h"
#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "components/data_reduction_proxy/proto/pageload_metrics.pb.h"
#include "net/base/network_change_notifier.h"
#include "net/base/proxy_server.h"
#include "net/nqe/effective_connection_type.h"
#include "net/proxy_resolution/proxy_retry_info.h"
#include "url/gurl.h"

namespace base {
class Time;
class TimeDelta;
}  // namespace base

namespace net {
class ProxyConfig;
class ProxyInfo;
class URLRequest;
}  // namespace net

namespace data_reduction_proxy {

class LoFiDecider;

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

// Scheme of the proxy used.
enum ProxyScheme {
  PROXY_SCHEME_UNKNOWN = 0,
  PROXY_SCHEME_HTTP,
  PROXY_SCHEME_HTTPS,
  PROXY_SCHEME_QUIC,
  PROXY_SCHEME_DIRECT,
  PROXY_SCHEME_MAX
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

// Returns whether this is valid for data reduction proxy use. |proxy_info|
// should contain a single DIRECT ProxyServer, |url| should not be WS or WSO,
// and the |method| should be idempotent for this to be eligible.
bool EligibleForDataReductionProxy(const net::ProxyInfo& proxy_info,
                                   const GURL& url,
                                   const std::string& method);

// Determines if |proxy_config| would override a direct. |proxy_config| should
// be a data reduction proxy config with proxy servers mapped in the
// rules, or DIRECT to indicate DRP is not to be used. |proxy_retry_info|
// contains the list of bad proxies. |url| is used to determine whether it is
// HTTP or HTTPS. |data_reduction_proxy_info| is an out param that will contain
// the proxies that should be used.
bool ApplyProxyConfigToProxyInfo(const net::ProxyConfig& proxy_config,
                                 const net::ProxyRetryInfoMap& proxy_retry_info,
                                 const GURL& url,
                                 net::ProxyInfo* data_reduction_proxy_info);

// Calculates the original content length (OCL) of the |request|, from the OFCL
// value in the Chrome-Proxy header. |request| must not be cached. This does not
// account for partial failed responses.
int64_t CalculateOCLFromOFCL(const net::URLRequest& request);

// Calculates the effective original content length of the |request|. For
// successful requests OCL will be obtained from OFCL if available or from
// received response length. For partial failed responses an estimate is
// provided by scaling received response length based on OFCL and Content-Length
// header.
int64_t EstimateOriginalBodySize(const net::URLRequest& request,
                                 const LoFiDecider* lofi_decider);

// Given a |request| that went through the Data Reduction Proxy; this function
// estimates how many bytes would have been received if the response had been
// received directly from the origin without any data saver optimizations.
int64_t EstimateOriginalReceivedBytes(const net::URLRequest& request,
                                      const LoFiDecider* lofi_decider);

// Converts net::ProxyServer::Scheme to type ProxyScheme.
ProxyScheme ConvertNetProxySchemeToProxyScheme(net::ProxyServer::Scheme scheme);

// Returns the hostname used for the other bucket to record datause not scoped
// to a page load such as chrome-services traffic, service worker, Downloads.
const char* GetSiteBreakdownOtherHostName();

}  // namespace util

namespace protobuf_parser {

static_assert(net::EFFECTIVE_CONNECTION_TYPE_LAST == 6,
              "If net::EFFECTIVE_CONNECTION_TYPE changes, "
              "PageloadMetrics_EffectiveConnectionType needs to be updated.");

// Returns the PageloadMetrics_EffectiveConnectionType equivalent of
// |effective_connection_type|.
PageloadMetrics_EffectiveConnectionType
ProtoEffectiveConnectionTypeFromEffectiveConnectionType(
    net::EffectiveConnectionType effective_connection_type);

// Returns the PageloadMetrics_ConnectionType equivalent of
// |connection_type|.
PageloadMetrics_ConnectionType ProtoConnectionTypeFromConnectionType(
    net::NetworkChangeNotifier::ConnectionType connection_type);

// Returns the RequestInfo_Protocol equivalent of |protocol|.
RequestInfo_Protocol ProtoRequestInfoProtocolFromRequestInfoProtocol(
    DataReductionProxyData::RequestInfo::Protocol protocol);

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
