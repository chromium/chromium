// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_bypass_protocol.h"

#include <vector>

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_bypass_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_server.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_type_info.h"
#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "net/base/proxy_server.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/proxy_resolution/proxy_retry_info.h"
#include "url/gurl.h"

namespace data_reduction_proxy {

namespace {

// Returns the Data Reduction Proxy servers in |proxy_type_info| that should be
// marked bad according to |data_reduction_proxy_info|.
std::vector<net::ProxyServer> GetProxiesToMarkBad(
    const DataReductionProxyInfo& data_reduction_proxy_info,
    const DataReductionProxyTypeInfo& proxy_type_info) {
  DCHECK_GT(proxy_type_info.proxy_servers.size(), proxy_type_info.proxy_index);

  const size_t bad_proxy_end_index = data_reduction_proxy_info.bypass_all
                                         ? proxy_type_info.proxy_servers.size()
                                         : proxy_type_info.proxy_index + 1U;

  std::vector<net::ProxyServer> bad_proxies;

  for (size_t i = proxy_type_info.proxy_index; i < bad_proxy_end_index; ++i) {
    const net::ProxyServer& bad_proxy =
        proxy_type_info.proxy_servers[i].proxy_server();
    DCHECK(bad_proxy.is_valid());
    DCHECK(!bad_proxy.is_direct());
    bad_proxies.push_back(bad_proxy);
  }

  return bad_proxies;
}

void ReportResponseProxyServerStatusHistogram(
    DataReductionProxyBypassProtocol::ResponseProxyServerStatus status) {
  UMA_HISTOGRAM_ENUMERATION(
      "DataReductionProxy.ResponseProxyServerStatus", status,
      DataReductionProxyBypassProtocol::RESPONSE_PROXY_SERVER_STATUS_MAX);
}

}  // namespace

DataReductionProxyBypassProtocol::DataReductionProxyBypassProtocol(
    DataReductionProxyConfig* config)
    : config_(config) {
  DCHECK(config_);
}

DataReductionProxyBypassProtocol::~DataReductionProxyBypassProtocol() {
}

bool DataReductionProxyBypassProtocol::MaybeBypassProxyAndPrepareToRetry(
    const std::string& method,
    const std::vector<GURL>& url_chain,
    const net::HttpResponseHeaders* response_headers,
    const net::ProxyServer& proxy_server,
    net::Error net_error,
    const net::ProxyRetryInfoMap& proxy_retry_info,
    DataReductionProxyBypassType* proxy_bypass_type,
    DataReductionProxyInfo* data_reduction_proxy_info,
    std::vector<net::ProxyServer>* bad_proxies,
    bool* should_bypass_proxy_and_cache) {
  DCHECK(thread_checker_.CalledOnValidThread());

  bad_proxies->clear();
  *should_bypass_proxy_and_cache = false;

  DataReductionProxyBypassType bypass_type;

  bool retry;

  std::vector<DataReductionProxyServer> placeholder_proxy_servers;
  base::Optional<DataReductionProxyTypeInfo> proxy_type_info =
      config_->FindConfiguredDataReductionProxy(proxy_server);

  if (!response_headers) {
    retry = HandleInvalidResponseHeadersCase(
        url_chain, net_error, proxy_type_info, data_reduction_proxy_info,
        &bypass_type);
  } else {
    if (!proxy_type_info) {
      if (!proxy_server.is_valid() || proxy_server.is_direct() ||
          proxy_server.host_port_pair().IsEmpty()) {
        ReportResponseProxyServerStatusHistogram(
            RESPONSE_PROXY_SERVER_STATUS_EMPTY);
        return false;
      }

      if (!HasDataReductionProxyViaHeader(*response_headers, nullptr)) {
        ReportResponseProxyServerStatusHistogram(
            RESPONSE_PROXY_SERVER_STATUS_NON_DRP_NO_VIA);
        return false;
      }

      ReportResponseProxyServerStatusHistogram(
          RESPONSE_PROXY_SERVER_STATUS_NON_DRP_WITH_VIA);

      // If the |proxy_server| doesn't match any of the currently configured
      // Data Reduction Proxies, but it still has the Data Reduction Proxy via
      // header, then apply the bypass logic regardless.
      // TODO(sclittle): Remove this workaround once http://crbug.com/876776 is
      // fixed.
      placeholder_proxy_servers.push_back(
          DataReductionProxyServer(proxy_server, ProxyServer_ProxyType_CORE));
      proxy_type_info.emplace(DataReductionProxyTypeInfo(
          placeholder_proxy_servers, 0U /* proxy_index */));
    } else {
      ReportResponseProxyServerStatusHistogram(
          RESPONSE_PROXY_SERVER_STATUS_DRP);
    }

    DCHECK(proxy_type_info);
    retry = HandleValidResponseHeadersCase(
        url_chain, response_headers, proxy_retry_info, *proxy_type_info,
        proxy_bypass_type, data_reduction_proxy_info, &bypass_type);
  }
  if (!retry)
    return false;

  if (data_reduction_proxy_info->mark_proxies_as_bad) {
    *bad_proxies =
        GetProxiesToMarkBad(*data_reduction_proxy_info, *proxy_type_info);
  } else {
    *should_bypass_proxy_and_cache = true;
  }

  return bypass_type == BYPASS_EVENT_TYPE_CURRENT || !response_headers ||
         net::HttpUtil::IsMethodIdempotent(method);
}

bool DataReductionProxyBypassProtocol::HandleInvalidResponseHeadersCase(
    const std::vector<GURL>& url_chain,
    net::Error net_error,
    const base::Optional<DataReductionProxyTypeInfo>&
        data_reduction_proxy_type_info,
    DataReductionProxyInfo* data_reduction_proxy_info,
    DataReductionProxyBypassType* bypass_type) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!data_reduction_proxy_type_info)
    return false;

  DCHECK(url_chain.back().SchemeIs(url::kHttpScheme));

  DCHECK_NE(net::OK, net_error);
  if (net_error == net::ERR_IO_PENDING ||
      net_error == net::ERR_NETWORK_CHANGED ||
      net_error == net::ERR_INTERNET_DISCONNECTED ||
      net_error == net::ERR_NETWORK_IO_SUSPENDED ||
      net_error == net::ERR_ABORTED ||
      net_error == net::ERR_INSUFFICIENT_RESOURCES ||
      net_error == net::ERR_OUT_OF_MEMORY ||
      net_error == net::ERR_NAME_NOT_RESOLVED ||
      net_error == net::ERR_NAME_RESOLUTION_FAILED ||
      net_error == net::ERR_ADDRESS_UNREACHABLE || std::abs(net_error) >= 400) {
    // No need to retry the request or mark the proxy as bad. Only bypass on
    // System related errors, connection related errors and certificate errors.
    return false;
  }

  static_assert(
      net::ERR_CONNECTION_RESET > -400 && net::ERR_SSL_PROTOCOL_ERROR > -400,
      "net error is not handled");

  base::UmaHistogramSparse(
      "DataReductionProxy.InvalidResponseHeadersReceived.NetError", -net_error);

  data_reduction_proxy_info->bypass_all = false;
  data_reduction_proxy_info->mark_proxies_as_bad = true;
  data_reduction_proxy_info->bypass_duration = base::TimeDelta::FromMinutes(5);
  data_reduction_proxy_info->bypass_action = BYPASS_ACTION_TYPE_BYPASS;
  *bypass_type = BYPASS_EVENT_TYPE_MEDIUM;

  return true;
}

bool DataReductionProxyBypassProtocol::HandleValidResponseHeadersCase(
    const std::vector<GURL>& url_chain,
    const net::HttpResponseHeaders* response_headers,
    const net::ProxyRetryInfoMap& proxy_retry_info,
    const DataReductionProxyTypeInfo& data_reduction_proxy_type_info,
    DataReductionProxyBypassType* proxy_bypass_type,
    DataReductionProxyInfo* data_reduction_proxy_info,
    DataReductionProxyBypassType* bypass_type) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Lack of headers implies either that the request was served from cache or
  // that request was served directly from the origin.
  DCHECK(response_headers);

  // At this point, the response is expected to have the data reduction proxy
  // via header, so detect and report cases where the via header is missing.
  DataReductionProxyBypassStats::DetectAndRecordMissingViaHeaderResponseCode(
      data_reduction_proxy_type_info.proxy_index == 0U, *response_headers);

  // GetDataReductionProxyBypassType will only log a net_log event if a bypass
  // command was sent via the data reduction proxy headers.
  *bypass_type = GetDataReductionProxyBypassType(url_chain, *response_headers,
                                                 data_reduction_proxy_info);

  if (proxy_bypass_type)
    *proxy_bypass_type = *bypass_type;
  if (*bypass_type == BYPASS_EVENT_TYPE_MAX)
    return false;

  DCHECK_GT(data_reduction_proxy_type_info.proxy_servers.size(),
            data_reduction_proxy_type_info.proxy_index);

  const net::ProxyServer& proxy_server =
      data_reduction_proxy_type_info
          .proxy_servers[data_reduction_proxy_type_info.proxy_index]
          .proxy_server();

  // Only record UMA if the proxy isn't already on the retry list.
  if (!config_->IsProxyBypassed(proxy_retry_info, proxy_server, nullptr)) {
    DataReductionProxyBypassStats::RecordDataReductionProxyBypassInfo(
        data_reduction_proxy_type_info.proxy_index == 0U,
        data_reduction_proxy_info->bypass_all, proxy_server, *bypass_type);
  }
  return true;
}

}  // namespace data_reduction_proxy
