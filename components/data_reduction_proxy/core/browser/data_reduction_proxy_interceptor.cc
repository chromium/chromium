// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_interceptor.h"

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_bypass_protocol.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_bypass_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_service_client.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "net/base/load_timing_info.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_http_job.h"
#include "net/url_request/url_request_job_manager.h"
#include "url/url_constants.h"

namespace data_reduction_proxy {

namespace {

void MarkProxiesAsBad(net::URLRequest* request,
                      const std::vector<net::ProxyServer>& bad_proxies,
                      base::TimeDelta bypass_duration) {
  // Synthesize a suitable |ProxyInfo| to add the proxies to the
  // |ProxyRetryInfoMap| of the proxy service.
  net::ProxyList proxy_list;
  for (const auto& bad_proxy : bad_proxies)
    proxy_list.AddProxyServer(bad_proxy);
  proxy_list.AddProxyServer(net::ProxyServer::Direct());

  net::ProxyInfo proxy_info;
  proxy_info.UseProxyList(proxy_list);

  request->context()->proxy_resolution_service()->MarkProxiesAsBadUntil(
      proxy_info, bypass_duration, bad_proxies, request->net_log());
}

}  // namespace

DataReductionProxyInterceptor::DataReductionProxyInterceptor(
    DataReductionProxyConfig* config,
    DataReductionProxyConfigServiceClient* config_service_client,
    DataReductionProxyBypassStats* stats)
    : bypass_stats_(stats),
      config_service_client_(config_service_client),
      bypass_protocol_(new DataReductionProxyBypassProtocol(config)) {}

DataReductionProxyInterceptor::~DataReductionProxyInterceptor() {
}

net::URLRequestJob* DataReductionProxyInterceptor::MaybeInterceptRequest(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  return nullptr;
}

net::URLRequestJob* DataReductionProxyInterceptor::MaybeInterceptRedirect(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const GURL& location) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  return MaybeInterceptResponseOrRedirect(request, network_delegate);
}

net::URLRequestJob* DataReductionProxyInterceptor::MaybeInterceptResponse(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  return MaybeInterceptResponseOrRedirect(request, network_delegate);
}

net::URLRequestJob*
DataReductionProxyInterceptor::MaybeInterceptResponseOrRedirect(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  DCHECK(request);
  if (request->response_info().was_cached)
    return nullptr;

  if (params::IsWarmupURL(request->url())) {
    // No need to retry fetch of warmup URLs since it is useful to fetch the
    // warmup URL only via a data saver proxy.
    return nullptr;
  }

  bool should_retry = false;
  // Consider retrying due to an authentication failure from the Data Reduction
  // Proxy server when using the config service.
  if (config_service_client_ != nullptr) {
    const net::HttpResponseHeaders* response_headers =
        request->response_info().headers.get();
    if (response_headers) {
      net::HttpRequestHeaders request_headers;
      net::LoadTimingInfo load_timing_info;
      request->GetLoadTimingInfo(&load_timing_info);
      if (request->GetFullRequestHeaders(&request_headers)) {
        should_retry = config_service_client_->ShouldRetryDueToAuthFailure(
            request_headers, response_headers, request->proxy_server(),
            load_timing_info);
      }
    }
  }

  // Consider retrying due errors stemming from the Data Reduction Proxy
  // protocol in the response headers.
  if (!should_retry) {
    DataReductionProxyInfo data_reduction_proxy_info;
    DataReductionProxyBypassType bypass_type = BYPASS_EVENT_TYPE_MAX;

    std::vector<net::ProxyServer> bad_proxies;
    bool should_bypass_proxy_and_cache;

    should_retry = bypass_protocol_->MaybeBypassProxyAndPrepareToRetry(
        request->method(), request->url_chain(), request->response_headers(),
        request->proxy_server(), request->status().ToNetError(),
        request->context()->proxy_resolution_service()->proxy_retry_info(),
        &bypass_type, &data_reduction_proxy_info, &bad_proxies,
        &should_bypass_proxy_and_cache);

    if (!bad_proxies.empty()) {
      MarkProxiesAsBad(request, bad_proxies,
                       data_reduction_proxy_info.bypass_duration);
    }

    if (should_bypass_proxy_and_cache) {
      request->SetLoadFlags(request->load_flags() | net::LOAD_BYPASS_CACHE |
                            net::LOAD_BYPASS_PROXY);
    }

    if (bypass_stats_ && bypass_type != BYPASS_EVENT_TYPE_MAX)
      bypass_stats_->SetBypassType(bypass_type);
  }

  DataReductionProxyData* data = DataReductionProxyData::GetData(*request);
  std::unique_ptr<DataReductionProxyData::RequestInfo> request_info =
      DataReductionProxyData::CreateRequestInfoFromRequest(request,
                                                           should_retry);
  if (data && request_info) {
    data->add_request_info(*request_info.get());
  }

  if (!should_retry)
    return nullptr;
  // Returning non-NULL has the effect of restarting the request with the
  // supplied job.
  DCHECK(request->url().SchemeIs(url::kHttpScheme));
  return net::URLRequestJobManager::GetInstance()->CreateJob(request,
                                                             network_delegate);
}

}  // namespace data_reduction_proxy
