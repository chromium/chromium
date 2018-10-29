// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_delegate.h"

#include <cmath>

#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_bypass_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_util.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "net/base/host_port_pair.h"
#include "net/base/proxy_server.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/proxy_resolution/proxy_info.h"

namespace data_reduction_proxy {

namespace {

static const char kDataReductionCoreProxy[] = "proxy.googlezip.net";

}  // namespace

DataReductionProxyDelegate::DataReductionProxyDelegate(
    DataReductionProxyConfig* config,
    const DataReductionProxyConfigurator* configurator,
    DataReductionProxyBypassStats* bypass_stats)
    : config_(config),
      configurator_(configurator),
      bypass_stats_(bypass_stats),
      io_data_(nullptr) {
  DCHECK(config_);
  DCHECK(configurator_);
  DCHECK(bypass_stats_);
  // Constructed on the UI thread, but should be checked on the IO thread.
  thread_checker_.DetachFromThread();
}

DataReductionProxyDelegate::~DataReductionProxyDelegate() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void DataReductionProxyDelegate::InitializeOnIOThread(
    DataReductionProxyIOData* io_data) {
  DCHECK(io_data);
  DCHECK(thread_checker_.CalledOnValidThread());
  io_data_ = io_data;
}

void DataReductionProxyDelegate::OnResolveProxy(
    const GURL& url,
    const std::string& method,
    const net::ProxyRetryInfoMap& proxy_retry_info,
    net::ProxyInfo* result) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(result);
  DCHECK(result->is_empty() || result->is_direct() ||
         !config_->FindConfiguredDataReductionProxy(result->proxy_server()));

  if (!params::IsIncludedInQuicFieldTrial())
    RecordQuicProxyStatus(QUIC_PROXY_DISABLED_VIA_FIELD_TRIAL);

  if (!util::EligibleForDataReductionProxy(*result, url, method))
    return;

  ResourceTypeProvider::ContentType content_type =
      ResourceTypeProvider::CONTENT_TYPE_UNKNOWN;
  if (io_data_ && io_data_->resource_type_provider())
    content_type = io_data_->resource_type_provider()->GetContentType(url);

  std::vector<DataReductionProxyServer> proxies_for_http =
      params::IsIncludedInHoldbackFieldTrial()
          ? std::vector<DataReductionProxyServer>()
          : config_->GetProxiesForHttp();

  // Remove the proxies that are unsupported for this request.
  base::EraseIf(proxies_for_http,
                [content_type](const DataReductionProxyServer& proxy) {
                  return !proxy.SupportsResourceType(content_type);
                });

  base::Optional<std::pair<bool /* is_secure_proxy */, bool /*is_core_proxy */>>
      warmup_proxy = config_->GetInFlightWarmupProxyDetails();

  bool is_warmup_url = warmup_proxy &&
                       url.host() == params::GetWarmupURL().host() &&
                       url.path() == params::GetWarmupURL().path();

  if (is_warmup_url) {
    // This is a request to fetch the warmup (aka probe) URL.
    // |is_secure_proxy| and |is_core_proxy| indicate the properties of the
    // proxy that is being currently probed.
    bool is_secure_proxy = warmup_proxy->first;
    bool is_core_proxy = warmup_proxy->second;
    // Remove the proxies with properties that do not match the properties of
    // the proxy that is being probed.
    base::EraseIf(proxies_for_http, [is_secure_proxy, is_core_proxy](
                                        const DataReductionProxyServer& proxy) {
      return proxy.IsSecureProxy() != is_secure_proxy ||
             proxy.IsCoreProxy() != is_core_proxy;
    });
  }

  // If the proxy is disabled due to warmup URL fetch failing in the past,
  // then enable it temporarily. This ensures that |configurator_| includes
  // this proxy type when generating the |proxy_config|.
  net::ProxyConfig proxy_config = configurator_->CreateProxyConfig(
      is_warmup_url, config_->GetNetworkPropertiesManager(), proxies_for_http);

  net::ProxyInfo data_reduction_proxy_info;
  if (util::ApplyProxyConfigToProxyInfo(proxy_config, proxy_retry_info, url,
                                        &data_reduction_proxy_info)) {
    DCHECK(!data_reduction_proxy_info.is_empty() &&
           !data_reduction_proxy_info.is_direct());
    result->OverrideProxyList(data_reduction_proxy_info.proxy_list());

    GetAlternativeProxy(url, proxy_retry_info, result);
  }

  DCHECK_GT(ResourceTypeProvider::CONTENT_TYPE_MAX, content_type);

  if (config_->enabled_by_user_and_reachable() &&
      url.SchemeIs(url::kHttpScheme) && !net::IsLocalhost(url) &&
      !params::IsIncludedInHoldbackFieldTrial()) {
    UMA_HISTOGRAM_BOOLEAN("DataReductionProxy.ConfigService.HTTPRequests",
                          !config_->GetProxiesForHttp().empty());
    if (content_type == ResourceTypeProvider::CONTENT_TYPE_MAIN_FRAME) {
      UMA_HISTOGRAM_BOOLEAN("DataReductionProxy.ConfigService.MainFrames",
                            !config_->GetProxiesForHttp().empty());
    }
  }
}

void DataReductionProxyDelegate::OnFallback(const net::ProxyServer& bad_proxy,
                                            int net_error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (bypass_stats_)
    bypass_stats_->OnProxyFallback(bad_proxy, net_error);
}

void DataReductionProxyDelegate::GetAlternativeProxy(
    const GURL& url,
    const net::ProxyRetryInfoMap& proxy_retry_info,
    net::ProxyInfo* result) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  net::ProxyServer resolved_proxy_server = result->proxy_server();
  DCHECK(resolved_proxy_server.is_valid());
  DCHECK(config_->FindConfiguredDataReductionProxy(resolved_proxy_server));

  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS() ||
      url.SchemeIsCryptographic()) {
    return;
  }

  if (!params::IsIncludedInQuicFieldTrial()) {
    // RecordQuicProxyStatus already called by OnResolveProxy.
    return;
  }

  if (!resolved_proxy_server.is_https())
    return;

  if (!SupportsQUIC(resolved_proxy_server)) {
    RecordQuicProxyStatus(QUIC_PROXY_NOT_SUPPORTED);
    return;
  }

  net::ProxyInfo alternative_proxy_info;
  alternative_proxy_info.UseProxyServer(net::ProxyServer(
      net::ProxyServer::SCHEME_QUIC, resolved_proxy_server.host_port_pair()));
  alternative_proxy_info.DeprioritizeBadProxies(proxy_retry_info);

  if (alternative_proxy_info.is_empty()) {
    RecordQuicProxyStatus(QUIC_PROXY_STATUS_MARKED_AS_BROKEN);
    return;
  }

  RecordQuicProxyStatus(QUIC_PROXY_STATUS_AVAILABLE);
  result->SetAlternativeProxy(alternative_proxy_info.proxy_server());
}

bool DataReductionProxyDelegate::SupportsQUIC(
    const net::ProxyServer& proxy_server) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Enable QUIC for whitelisted proxies.
  return params::IsQuicEnabledForNonCoreProxies() ||
         proxy_server ==
             net::ProxyServer(net::ProxyServer::SCHEME_HTTPS,
                              net::HostPortPair(kDataReductionCoreProxy, 443));
}

void DataReductionProxyDelegate::RecordQuicProxyStatus(
    QuicProxyStatus status) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  UMA_HISTOGRAM_ENUMERATION("DataReductionProxy.Quic.ProxyStatus", status,
                            QUIC_PROXY_STATUS_BOUNDARY);
}

}  // namespace data_reduction_proxy
