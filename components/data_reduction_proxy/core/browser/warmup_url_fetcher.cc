// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/warmup_url_fetcher.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/guid.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_util.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_server.h"
#include "components/data_reduction_proxy/core/common/uma_util.h"
#include "content/public/browser/network_service_instance.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace data_reduction_proxy {

namespace {

const int kInvalidResponseCode = -1;

void BindNetworkContext(
    network::mojom::CustomProxyConfigPtr config,
    mojo::PendingReceiver<network::mojom::NetworkContext> receiver,
    const std::string& user_agent) {
  auto params = network::mojom::NetworkContextParams::New();
  params->user_agent = user_agent;
  params->initial_custom_proxy_config = std::move(config);
  content::GetNetworkService()->CreateNetworkContext(std::move(receiver),
                                                     std::move(params));
}
}

WarmupURLFetcher::WarmupURLFetcher(
    CreateCustomProxyConfigCallback create_custom_proxy_config_callback,
    WarmupURLFetcherCallback callback,
    GetHttpRttCallback get_http_rtt_callback,
    const std::string& user_agent)
    : is_fetch_in_flight_(false),
      previous_attempt_counts_(0),
      create_custom_proxy_config_callback_(create_custom_proxy_config_callback),
      callback_(callback),
      get_http_rtt_callback_(get_http_rtt_callback),
      user_agent_(user_agent) {
  DCHECK(create_custom_proxy_config_callback);
  DCHECK(!params::IsIncludedInHoldbackFieldTrial());
}

WarmupURLFetcher::~WarmupURLFetcher() {}

void WarmupURLFetcher::FetchWarmupURL(
    size_t previous_attempt_counts,
    const DataReductionProxyServer& proxy_server) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!params::IsIncludedInHoldbackFieldTrial());

  previous_attempt_counts_ = previous_attempt_counts;

  DCHECK_LE(0u, previous_attempt_counts_);
  DCHECK_GE(2u, previous_attempt_counts_);

  // There can be at most one pending fetch at any time.
  fetch_delay_timer_.Stop();

  if (previous_attempt_counts_ == 0) {
    FetchWarmupURLNow(proxy_server);
    return;
  }
  fetch_delay_timer_.Start(
      FROM_HERE, GetFetchWaitTime(),
      base::BindOnce(&WarmupURLFetcher::FetchWarmupURLNow,
                     base::Unretained(this), proxy_server));
}

base::TimeDelta WarmupURLFetcher::GetFetchWaitTime() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_LT(0u, previous_attempt_counts_);
  DCHECK_GE(2u, previous_attempt_counts_);

  if (previous_attempt_counts_ == 1)
    return base::TimeDelta::FromSeconds(1);

  return base::TimeDelta::FromSeconds(30);
}

void WarmupURLFetcher::FetchWarmupURLNow(
    const DataReductionProxyServer& proxy_server) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!params::IsIncludedInHoldbackFieldTrial());

  UMA_HISTOGRAM_EXACT_LINEAR("DataReductionProxy.WarmupURL.FetchInitiated", 1,
                             2);
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("data_reduction_proxy_warmup", R"(
          semantics {
            sender: "Data Reduction Proxy"
            description:
              "Sends a request to the Data Reduction Proxy server to warm up "
              "the connection to the proxy."
            trigger:
              "A network change while the data reduction proxy is enabled will "
              "trigger this request."
            data: "A specific URL, not related to user data."
            destination: GOOGLE_OWNED_SERVICE
          }
          policy {
            cookies_allowed: NO
            setting:
              "Users can control Data Saver on Android via the 'Data Saver' "
              "setting. Data Saver is not available on iOS, and on desktop it "
              "is enabled by installing the Data Saver extension."
            policy_exception_justification: "Not implemented."
          })");

  GURL warmup_url_with_query_params;
  GetWarmupURLWithQueryParam(&warmup_url_with_query_params);

  url_loader_.reset();
  fetch_timeout_timer_.Stop();
  is_fetch_in_flight_ = true;

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = warmup_url_with_query_params;
  // Do not disable cookies. This allows the warmup connection to be reused
  // for loading user initiated requests.
  resource_request->load_flags = net::LOAD_BYPASS_CACHE;

  // TODO(957215): This is a temporary solution to mark the request to go
  // through the data reduction proxy. Otherwise only navigation requests and
  // renderer requests will be allowed to use the proxy.
  resource_request->render_frame_id = MSG_ROUTING_CONTROL;

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);
  // |url_loader_| should not retry on 5xx errors. |url_loader_| should retry on
  // network changes since the network stack may receive the connection change
  // event later than |this|.
  static const int kMaxRetries = 5;
  url_loader_->SetRetryOptions(
      kMaxRetries, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  url_loader_->SetAllowHttpErrorResults(true);

  fetch_timeout_timer_.Start(FROM_HERE, GetFetchTimeout(), this,
                             &WarmupURLFetcher::OnFetchTimeout);

  url_loader_->SetOnResponseStartedCallback(base::BindOnce(
      &WarmupURLFetcher::OnURLLoadResponseStarted, base::Unretained(this)));
  url_loader_->SetOnRedirectCallback(base::BindRepeating(
      &WarmupURLFetcher::OnURLLoaderRedirect, base::Unretained(this)));

  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      GetNetworkServiceURLLoaderFactory(proxy_server),
      base::BindOnce(&WarmupURLFetcher::OnURLLoadComplete,
                     base::Unretained(this)));
}

network::mojom::URLLoaderFactory*
WarmupURLFetcher::GetNetworkServiceURLLoaderFactory(
    const DataReductionProxyServer& proxy_server) {
  context_.reset();
  BindNetworkContext(create_custom_proxy_config_callback_.Run({proxy_server}),
                     context_.BindNewPipeAndPassReceiver(), user_agent_);

  auto factory_params = network::mojom::URLLoaderFactoryParams::New();
  factory_params->process_id = network::mojom::kBrowserProcessId;
  factory_params->is_corb_enabled = false;
  url_loader_factory_.reset();
  context_->CreateURLLoaderFactory(
      url_loader_factory_.BindNewPipeAndPassReceiver(),
      std::move(factory_params));
  return url_loader_factory_.get();
}

void WarmupURLFetcher::GetWarmupURLWithQueryParam(
    GURL* warmup_url_with_query_params) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Set the query param to a random string to prevent intermediate middleboxes
  // from returning cached content.
  const std::string query = "q=" + base::GenerateGUID();
  GURL::Replacements replacements;
  replacements.SetQuery(query.c_str(), url::Component(0, query.length()));

  *warmup_url_with_query_params =
      params::GetWarmupURL().ReplaceComponents(replacements);

  DCHECK(warmup_url_with_query_params->is_valid() &&
         warmup_url_with_query_params->has_query());
}

void WarmupURLFetcher::OnURLLoadResponseStarted(
    const GURL& final_url,
    const network::mojom::URLResponseHead& response_head) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  proxy_server_ = response_head.proxy_server;
}

void WarmupURLFetcher::OnURLLoaderRedirect(
    const net::RedirectInfo& redirect_info,
    const network::mojom::URLResponseHead& response_head,
    std::vector<std::string>* to_be_removed_headers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  proxy_server_ = response_head.proxy_server;
}

void WarmupURLFetcher::OnURLLoadComplete(
    std::unique_ptr<std::string> response_body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_fetch_in_flight_);

  UMA_HISTOGRAM_BOOLEAN("DataReductionProxy.WarmupURL.FetchSuccessful",
                        !!response_body);

  base::UmaHistogramSparse("DataReductionProxy.WarmupURL.NetError",
                           std::abs(url_loader_->NetError()));

  scoped_refptr<net::HttpResponseHeaders> response_headers;
  int response_code = kInvalidResponseCode;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
    response_headers = url_loader_->ResponseInfo()->headers;
    response_code = response_headers->response_code();
  }

  base::UmaHistogramSparse("DataReductionProxy.WarmupURL.HttpResponseCode",
                           std::abs(response_code));

  if (response_headers) {
    UMA_HISTOGRAM_BOOLEAN(
        "DataReductionProxy.WarmupURL.HasViaHeader",
        HasDataReductionProxyViaHeader(*response_headers,
                                       nullptr /* has_intermediary */));
    UMA_HISTOGRAM_ENUMERATION(
        "DataReductionProxy.WarmupURL.ProxySchemeUsed",
        ConvertNetProxySchemeToProxyScheme(proxy_server_.scheme()),
        PROXY_SCHEME_MAX);
  }

  if (!params::IsWarmupURLFetchCallbackEnabled()) {
    CleanupAfterFetch();
    return;
  }

  if (url_loader_->NetError() == net::ERR_INTERNET_DISCONNECTED) {
    // Fetching failed due to Internet unavailability, and not due to some
    // error. No need to run the callback.
    CleanupAfterFetch();
    return;
  }

  bool success_response =
      response_body &&
      params::IsWhitelistedHttpResponseCodeForProbes(response_code) &&
      response_headers &&
      HasDataReductionProxyViaHeader(*response_headers,
                                     nullptr /* has_intermediary */);
  callback_.Run(proxy_server_, success_response ? FetchResult::kSuccessful
                                                : FetchResult::kFailed);
  CleanupAfterFetch();
}

bool WarmupURLFetcher::IsFetchInFlight() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return is_fetch_in_flight_;
}

base::TimeDelta WarmupURLFetcher::GetFetchTimeout() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_LE(0u, previous_attempt_counts_);
  DCHECK_GE(2u, previous_attempt_counts_);

  // The timeout value should always be between |min_timeout| and |max_timeout|
  // (both inclusive).
  const base::TimeDelta min_timeout = base::TimeDelta::FromSeconds(30);
  const base::TimeDelta max_timeout = base::TimeDelta::FromSeconds(60);
  DCHECK_LT(base::TimeDelta::FromSeconds(0), min_timeout);
  DCHECK_LT(base::TimeDelta::FromSeconds(0), max_timeout);
  DCHECK_LE(min_timeout, max_timeout);

  // Set the timeout based on how many times the fetching of the warmup URL
  // has been tried.
  size_t http_rtt_multiplier = 12;
  if (previous_attempt_counts_ == 1) {
    http_rtt_multiplier *= 2;
  } else if (previous_attempt_counts_ == 2) {
    http_rtt_multiplier *= 4;
  }
  // Sanity checks.
  DCHECK_LT(0u, http_rtt_multiplier);
  DCHECK_GE(1000u, http_rtt_multiplier);

  base::Optional<base::TimeDelta> http_rtt_estimate =
      get_http_rtt_callback_.Run();
  if (!http_rtt_estimate)
    return max_timeout;

  base::TimeDelta timeout = http_rtt_multiplier * http_rtt_estimate.value();
  if (timeout > max_timeout)
    return max_timeout;

  if (timeout < min_timeout)
    return min_timeout;

  return timeout;
}

void WarmupURLFetcher::OnFetchTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_fetch_in_flight_);
  DCHECK(url_loader_);

  DCHECK_LE(1, proxy_server_.scheme());

  UMA_HISTOGRAM_BOOLEAN("DataReductionProxy.WarmupURL.FetchSuccessful", false);
  base::UmaHistogramSparse("DataReductionProxy.WarmupURL.NetError",
                           net::ERR_ABORTED);
  base::UmaHistogramSparse("DataReductionProxy.WarmupURL.HttpResponseCode",
                           std::abs(kInvalidResponseCode));

  if (!params::IsWarmupURLFetchCallbackEnabled()) {
    // Running the callback is not enabled.
    CleanupAfterFetch();
    return;
  }

  callback_.Run(proxy_server_, FetchResult::kTimedOut);
  CleanupAfterFetch();
}

void WarmupURLFetcher::CleanupAfterFetch() {
  is_fetch_in_flight_ = false;
  url_loader_.reset();
  proxy_server_ = net::ProxyServer();
  fetch_timeout_timer_.Stop();
  fetch_delay_timer_.Stop();
}

}  // namespace data_reduction_proxy
