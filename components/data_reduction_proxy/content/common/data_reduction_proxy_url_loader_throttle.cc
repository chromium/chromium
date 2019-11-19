// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/content/common/data_reduction_proxy_url_loader_throttle.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "components/data_reduction_proxy/content/common/header_util.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_bypass_protocol.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_server.h"
#include "components/data_reduction_proxy/core/common/uma_util.h"
#include "net/base/load_flags.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace net {
class HttpRequestHeaders;
}

namespace data_reduction_proxy {

namespace {
void RecordQuicProxyStatus(const net::ProxyServer& proxy_server) {
  if (proxy_server.is_https() || proxy_server.is_quic()) {
    RecordQuicProxyStatus(IsQuicProxy(proxy_server)
                              ? QUIC_PROXY_STATUS_AVAILABLE
                              : QUIC_PROXY_NOT_SUPPORTED);
  }
}

}  // namespace

DataReductionProxyURLLoaderThrottle::DataReductionProxyURLLoaderThrottle(
    const net::HttpRequestHeaders& post_cache_headers,
    DataReductionProxyThrottleManager* manager)
    : post_cache_headers_(post_cache_headers),
      manager_(manager),
      data_reduction_proxy_(manager_->data_reduction_proxy()) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(manager_);

  manager_->AddSameSequenceObserver(this);
  OnThrottleConfigChanged(manager_->last_proxy_config());
}

DataReductionProxyURLLoaderThrottle::~DataReductionProxyURLLoaderThrottle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (manager_)
    manager_->RemoveSameSequenceObserver(this);
}

void DataReductionProxyURLLoaderThrottle::DetachFromCurrentSequence() {
  DETACH_FROM_SEQUENCE(sequence_checker_);

  if (manager_) {
    manager_->RemoveSameSequenceObserver(this);
    manager_ = nullptr;
  }

  data_reduction_proxy_->Clone(
      private_data_reduction_proxy_remote_.InitWithNewPipeAndPassReceiver());
  data_reduction_proxy_ = nullptr;
}

void DataReductionProxyURLLoaderThrottle::SetUpPrivateMojoPipes() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(data_reduction_proxy_, nullptr);

  // Bind the pipe created in DetachFromCurrentSequence() to the current
  // sequence.
  private_data_reduction_proxy_.Bind(
      std::move(private_data_reduction_proxy_remote_));
  data_reduction_proxy_ = private_data_reduction_proxy_.get();

  data_reduction_proxy_->AddThrottleConfigObserver(
      private_config_observer_receiver_.BindNewPipeAndPassRemote());
}

void DataReductionProxyURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (private_data_reduction_proxy_remote_)
    SetUpPrivateMojoPipes();

  url_chain_.clear();
  url_chain_.push_back(request->url);
  request_method_ = request->method;
  is_main_frame_ = request->resource_type ==
                   static_cast<int>(content::ResourceType::kMainFrame);
  final_load_flags_ = request->load_flags;

  MaybeSetAcceptTransformHeader(
      request->url, static_cast<content::ResourceType>(request->resource_type),
      request->previews_state, &request->custom_proxy_pre_cache_headers);
  request->custom_proxy_post_cache_headers = post_cache_headers_;

  if (request->resource_type == static_cast<int>(content::ResourceType::kMedia))
    request->custom_proxy_use_alternate_proxy_list = true;
}

void DataReductionProxyURLLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    bool* defer,
    std::vector<std::string>* to_be_removed_request_headers,
    net::HttpRequestHeaders* modified_request_headers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  url_chain_.push_back(redirect_info->new_url);
  request_method_ = redirect_info->new_method;
}

void DataReductionProxyURLLoaderThrottle::BeforeWillProcessResponse(
    const GURL& response_url,
    const network::mojom::URLResponseHead& response_head,
    bool* defer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  before_will_process_response_received_ = true;
  if (response_head.was_fetched_via_cache)
    return;

  DCHECK_EQ(response_url, url_chain_.back());
  DCHECK(!pending_restart_);

  const net::ProxyServer& proxy_server = response_head.proxy_server;

  // No need to retry fetch of warmup URLs since it is useful to fetch the
  // warmup URL only via a data saver proxy.
  if (params::IsWarmupURL(response_url))
    return;

  MaybeRetry(proxy_server, response_head.headers.get(), net::OK, defer);
  RecordQuicProxyStatus(proxy_server);
}

void DataReductionProxyURLLoaderThrottle::MaybeRetry(
    const net::ProxyServer& proxy_server,
    const net::HttpResponseHeaders* headers,
    net::Error net_error,
    bool* defer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The set of data reduction proxy servers to mark as bad prior to
  // restarting the request.
  std::vector<net::ProxyServer> bad_proxies;

  // TODO(https://crbug.com/721403): Implement retry due to authentication
  // failure.

  // TODO(https://crbug.com/721403): Need the actual bad proxies map. Since
  // this is only being used for some metrics logging not a big deal.
  net::ProxyRetryInfoMap proxy_retry_info;

  DataReductionProxyInfo data_reduction_proxy_info;

  DataReductionProxyBypassType bypass_type = BYPASS_EVENT_TYPE_MAX;

  DataReductionProxyBypassProtocol protocol;
  pending_restart_ = protocol.MaybeBypassProxyAndPrepareToRetry(
      request_method_, url_chain_, headers, proxy_server, net_error,
      proxy_retry_info, FindConfiguredDataReductionProxy(proxy_server),
      &bypass_type, &data_reduction_proxy_info, &bad_proxies,
      &pending_restart_load_flags_);

  if (!bad_proxies.empty())
    MarkProxiesAsBad(bad_proxies, data_reduction_proxy_info.bypass_duration);

  // TODO(https://crbug.com/721403): Log bypass stats.

  // If proxies are being marked as bad the throttle needs to defer. The
  // throttle will later be resumed  (and possibly restartd) in
  // OnMarkProxiesAsBadComplete()).
  if (waiting_for_mark_proxies_) {
    *defer = true;
  } else {
    DoPendingRestart();
  }
}

void DataReductionProxyURLLoaderThrottle::WillProcessResponse(
    const GURL& response_url,
    network::mojom::URLResponseHead* response_head,
    bool* defer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::Optional<DataReductionProxyTypeInfo> proxy_info =
      FindConfiguredDataReductionProxy(response_head->proxy_server);
  if (!proxy_info || (final_load_flags_ & net::LOAD_BYPASS_PROXY) != 0)
    return;

  LogSuccessfulProxyUMAs(proxy_info.value(), response_head->proxy_server,
                         is_main_frame_);
}

void DataReductionProxyURLLoaderThrottle::WillOnCompleteWithError(
    const network::URLLoaderCompletionStatus& status,
    bool* defer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!before_will_process_response_received_) {
    MaybeRetry(status.proxy_server, nullptr,
               static_cast<net::Error>(status.error_code), defer);
  }
}

void DataReductionProxyURLLoaderThrottle::MarkProxiesAsBad(
    const std::vector<net::ProxyServer>& bad_proxies,
    base::TimeDelta bypass_duration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!waiting_for_mark_proxies_);
  DCHECK(!bad_proxies.empty());

  // Convert |bad_proxies| to a net::ProxyList that is expected by the mojo
  // interface.
  net::ProxyList proxy_list;
  for (const auto& proxy : bad_proxies)
    proxy_list.AddProxyServer(proxy);

  auto callback = base::BindOnce(
      &DataReductionProxyURLLoaderThrottle::OnMarkProxiesAsBadComplete,
      weak_factory_.GetWeakPtr());

  waiting_for_mark_proxies_ = true;

  // There is no need to handle the case where |callback| is never invoked
  // (possible on connection error). That would imply disconnection from the
  // browser, which is not recoverable.
  data_reduction_proxy_->MarkProxiesAsBad(bypass_duration, proxy_list,
                                          std::move(callback));
}

void DataReductionProxyURLLoaderThrottle::OnMarkProxiesAsBadComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(waiting_for_mark_proxies_);

  waiting_for_mark_proxies_ = false;

  DoPendingRestart();

  // Un-defer the throttle.
  delegate_->Resume();
}

void DataReductionProxyURLLoaderThrottle::OnThrottleConfigChanged(
    mojom::DataReductionProxyThrottleConfigPtr config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  proxies_for_http_.clear();

  if (!config)
    return;

  // TODO(eroman): Use typemappings instead of converting here?
  for (const auto& entry : config->proxies_for_http) {
    proxies_for_http_.push_back(DataReductionProxyServer(entry->proxy_server));
  }
}

void DataReductionProxyURLLoaderThrottle::OnThrottleManagerDestroyed(
    DataReductionProxyThrottleManager* manager) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(manager, manager_);
  manager_->RemoveSameSequenceObserver(this);
  manager_ = nullptr;
}

base::Optional<DataReductionProxyTypeInfo>
DataReductionProxyURLLoaderThrottle::FindConfiguredDataReductionProxy(
    const net::ProxyServer& proxy_server) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(https://crbug.com/721403): The non-NS code also searches through the
  // recently seen proxies, not just the current ones.
  return params::FindConfiguredProxyInVector(proxies_for_http_, proxy_server);
}

void DataReductionProxyURLLoaderThrottle::DoPendingRestart() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!pending_restart_)
    return;

  int load_flags = pending_restart_load_flags_;

  pending_restart_ = false;
  pending_restart_load_flags_ = 0;
  final_load_flags_ |= load_flags;

  delegate_->RestartWithFlags(load_flags);
}

}  // namespace data_reduction_proxy
