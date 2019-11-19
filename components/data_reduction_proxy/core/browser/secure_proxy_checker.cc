// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/secure_proxy_checker.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

// Key of the UMA DataReductionProxy.SecureProxyCheck.Latency histogram.
const char kUMAProxySecureProxyCheckLatency[] =
    "DataReductionProxy.SecureProxyCheck.Latency";

}  // namespace

namespace data_reduction_proxy {

SecureProxyChecker::SecureProxyChecker(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)) {
  DCHECK(!params::IsIncludedInHoldbackFieldTrial());
}

void SecureProxyChecker::OnURLLoadComplete(
    std::unique_ptr<std::string> response_body) {
  std::string response;
  if (response_body)
    response = std::move(*response_body);

  int response_code = -1;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers)
    response_code = url_loader_->ResponseInfo()->headers->response_code();

  OnURLLoadCompleteOrRedirect(response, url_loader_->NetError(), response_code);
}

void SecureProxyChecker::OnURLLoadCompleteOrRedirect(
    const std::string& response,
    int net_error,
    int response_code) {
  url_loader_.reset();

  base::TimeDelta secure_proxy_check_latency =
      base::Time::Now() - secure_proxy_check_start_time_;
  if (secure_proxy_check_latency >= base::TimeDelta()) {
    UMA_HISTOGRAM_MEDIUM_TIMES(kUMAProxySecureProxyCheckLatency,
                               secure_proxy_check_latency);
  }

  fetcher_callback_.Run(response, net_error, response_code);
}

void SecureProxyChecker::OnURLLoaderRedirect(
    const net::RedirectInfo& redirect_info,
    const network::mojom::URLResponseHead& response_head,
    std::vector<std::string>* to_be_removed_headers) {
  OnURLLoadCompleteOrRedirect(std::string(), net::ERR_ABORTED,
                              redirect_info.status_code);
}

void SecureProxyChecker::CheckIfSecureProxyIsAllowed(
    SecureProxyCheckerCallback fetcher_callback) {
  DCHECK(!params::IsIncludedInHoldbackFieldTrial());

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation(
          "data_reduction_proxy_secure_proxy_check", R"(
            semantics {
              sender: "Data Reduction Proxy"
              description:
                "Sends a request to the Data Reduction Proxy server. Proceeds "
                "with using a secure connection to the proxy only if the "
                "response is not blocked or modified by an intermediary."
              trigger:
                "A request can be sent whenever the browser is determining how "
                "to configure its connection to the data reduction proxy. This "
                "happens on startup and network changes."
              data: "A specific URL, not related to user data."
              destination: GOOGLE_OWNED_SERVICE
            }
            policy {
              cookies_allowed: NO
              setting:
                "Users can control Data Saver on Android via the 'Data Saver' "
                "setting. Data Saver is not available on iOS, and on desktop "
                "it is enabled by installing the Data Saver extension."
              policy_exception_justification: "Not implemented."
            })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = params::GetSecureProxyCheckURL();
  resource_request->load_flags =
      net::LOAD_DISABLE_CACHE | net::LOAD_BYPASS_PROXY;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);

  // Configure max retries to be at most kMaxRetries times for 5xx errors.
  static const int kMaxRetries = 5;
  url_loader_->SetRetryOptions(
      kMaxRetries, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE |
                       network::SimpleURLLoader::RETRY_ON_5XX);
  // Hook the redirect callback so we can cancel the request.
  // The secure proxy check should not be redirected. Since the secure proxy
  // check will inevitably fail if it gets redirected somewhere else (e.g. by
  // a captive portal), short circuit that by giving up on the secure proxy
  // check if it gets redirected.
  url_loader_->SetOnRedirectCallback(base::BindRepeating(
      &SecureProxyChecker::OnURLLoaderRedirect, base::Unretained(this)));

  fetcher_callback_ = fetcher_callback;
  secure_proxy_check_start_time_ = base::Time::Now();

  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&SecureProxyChecker::OnURLLoadComplete,
                     base::Unretained(this)));
}

SecureProxyChecker::~SecureProxyChecker() {}

}  // namespace data_reduction_proxy
