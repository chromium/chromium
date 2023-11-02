// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safety_check/update_check_helper.h"

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/safety_check/url_constants.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace safety_check {

namespace {

// Request timeout of 5 seconds to not interrupt the completion of Safety check.
// The user can always start a new Safety check if a request times out.
constexpr base::TimeDelta kConnectionTimeout = base::Seconds(5);

// Maximum number of retries for sending the request.
constexpr int kMaxRetries = 2;

const net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("safety_check_update_connectivity",
                                        R"(
      semantics {
        sender: "Safety Check Browser Updates Check"
        description:
          "If during the updates check part of the Safety check the version "
          "updater returns a generic error status, this request is used to "
          "determine whether it is caused by connectivity issues."
        trigger:
          "When the user started the Safety check in settings and the browser "
          "updates check fails."
        data:
          "No data is sent with the request."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        cookies_allowed: NO
        setting:
          "No user-visible setting for this feature because this is part of "
          "user-triggered Safety check, which explicitly includes an update "
          "check."
        policy_exception_justification:
          "Not implemented, considered not required."
      })");

}  // namespace

UpdateCheckHelper::UpdateCheckHelper(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory) {}

UpdateCheckHelper::~UpdateCheckHelper() = default;

void UpdateCheckHelper::CheckConnectivity(
    ConnectivityCheckCallback connection_check_callback) {
  result_callback_ = std::move(connection_check_callback);

  // Create a request with no data or cookies.
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(kConnectivityCheckUrl);
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  resource_request->redirect_mode = ::network::mojom::RedirectMode::kError;

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 kTrafficAnnotation);
  url_loader_->SetTimeoutDuration(kConnectionTimeout);
  url_loader_->SetRetryOptions(
      kMaxRetries,
      network::SimpleURLLoader::RetryMode::RETRY_ON_5XX |
          network::SimpleURLLoader::RetryMode::RETRY_ON_NETWORK_CHANGE);
  url_loader_->DownloadHeadersOnly(
      url_loader_factory_.get(),
      base::BindOnce(&UpdateCheckHelper::OnURLLoadComplete,
                     base::Unretained(this)));
}

UpdateCheckHelper::UpdateCheckHelper() = default;

void UpdateCheckHelper::OnURLLoadComplete(
    scoped_refptr<net::HttpResponseHeaders> headers) {
  DCHECK(url_loader_);
  bool connected = headers && headers->response_code() == net::HTTP_NO_CONTENT;
  url_loader_.reset();
  std::move(result_callback_).Run(connected);
}

}  // namespace safety_check
