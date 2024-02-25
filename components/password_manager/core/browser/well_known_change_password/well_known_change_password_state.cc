// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/well_known_change_password/well_known_change_password_state.h"

#include <optional>
#include <utility>

#include "base/time/time.h"
#include "components/affiliations/core/browser/affiliation_service.h"
#include "components/password_manager/core/browser/well_known_change_password/well_known_change_password_util.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/origin.h"

using password_manager::WellKnownChangePasswordState;
using password_manager::WellKnownChangePasswordStateDelegate;

namespace password_manager {

namespace {
// Creates a SimpleURLLoader for a request to the non existing resource path for
// a given |url|.
std::unique_ptr<network::SimpleURLLoader>
CreateResourceRequestToWellKnownNonExistingResourceFor(
    const GURL& url,
    std::optional<url::Origin> request_initiator,
    std::optional<network::ResourceRequest::TrustedParams> trusted_params) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = CreateWellKnownNonExistingResourceURL(url);
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  resource_request->request_initiator = std::move(request_initiator);
  resource_request->trusted_params = std::move(trusted_params);
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation(
          "well_known_path_that_should_not_exist",
          R"(
        semantics {
          sender: "Password Manager"
          description:
            "Check whether the site supports .well-known 'special' URLs."
            "If the website does not support the spec we navigate to the "
            "fallback url. See also "
"https://wicg.github.io/change-password-url/response-code-reliability.html#iana"
          trigger:
            "When the user clicks 'Change password' on "
            "chrome://settings/passwords, or when they visit the "
            "[ORIGIN]/.well-known/change-password special URL, Chrome makes "
            "this additional request. Chrome Password manager shows a button "
            "with the link in the password checkup for compromised passwords "
            "view (chrome://password-manager/checkup/compromised) and in a dialog when the "
            "user signs in using compromised credentials."
          data:
            "The request body is empty. No user data is included."
          destination: WEBSITE
          internal {
            contacts {
              email: "chrome-worker@google.com"
            }
          }
          user_data {
            type: NONE
          }
          last_reviewed: "2023-09-22"
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled."
          policy_exception_justification: "Essential for navigation."
        })");
  return network::SimpleURLLoader::Create(std::move(resource_request),
                                          traffic_annotation);
}
}  // namespace

constexpr base::TimeDelta WellKnownChangePasswordState::kPrefetchTimeout;

WellKnownChangePasswordState::WellKnownChangePasswordState(
    WellKnownChangePasswordStateDelegate* delegate)
    : delegate_(delegate) {}

WellKnownChangePasswordState::~WellKnownChangePasswordState() = default;

void WellKnownChangePasswordState::FetchNonExistingResource(
    network::SharedURLLoaderFactory* url_loader_factory,
    const GURL& url,
    std::optional<url::Origin> request_initiator,
    std::optional<network::ResourceRequest::TrustedParams> trusted_params) {
  url_loader_ = CreateResourceRequestToWellKnownNonExistingResourceFor(
      url, std::move(request_initiator), std::move(trusted_params));
  // Binding the callback to |this| is safe, because the State exists until
  // OnProcessingFinished is called which can only be called after the response
  // arrives.
  url_loader_->DownloadHeadersOnly(
      url_loader_factory,
      base::BindOnce(
          &WellKnownChangePasswordState::FetchNonExistingResourceCallback,
          base::Unretained(this)));
}

void WellKnownChangePasswordState::PrefetchChangePasswordURLs(
    affiliations::AffiliationService* affiliation_service,
    const std::vector<GURL>& urls) {
  prefetch_timer_.Start(FROM_HERE, kPrefetchTimeout, this,
                        &WellKnownChangePasswordState::ContinueProcessing);
  affiliation_service->PrefetchChangePasswordURLs(
      urls,
      base::BindOnce(
          &WellKnownChangePasswordState::PrefetchChangePasswordURLsCallback,
          weak_factory_.GetWeakPtr()));
}

void WellKnownChangePasswordState::SetChangePasswordResponseCode(
    int status_code) {
  change_password_response_code_ = status_code;
  ContinueProcessing();
}

void WellKnownChangePasswordState::FetchNonExistingResourceCallback(
    scoped_refptr<net::HttpResponseHeaders> headers) {
  non_existing_resource_response_code_ =
      headers ? headers->response_code() : -1;
  ContinueProcessing();
}

void WellKnownChangePasswordState::PrefetchChangePasswordURLsCallback() {
  if (prefetch_timer_.IsRunning()) {
    prefetch_timer_.Stop();
    ContinueProcessing();
  }
}

void WellKnownChangePasswordState::ContinueProcessing() {
  if (BothRequestsFinished()) {
    bool is_well_known_supported = SupportsWellKnownChangePasswordUrl();
    // Don't wait for change password URL from Affiliation Service if
    // .well-known/change-password is supported.
    if (is_well_known_supported || !prefetch_timer_.IsRunning()) {
      delegate_->OnProcessingFinished(is_well_known_supported);
    }
  }
}

bool WellKnownChangePasswordState::BothRequestsFinished() const {
  return non_existing_resource_response_code_ != 0 &&
         change_password_response_code_ != 0;
}

bool WellKnownChangePasswordState::SupportsWellKnownChangePasswordUrl() const {
  DCHECK(BothRequestsFinished());
  return 200 <= change_password_response_code_ &&
         change_password_response_code_ < 300 &&
         non_existing_resource_response_code_ == net::HTTP_NOT_FOUND;
}

}  // namespace password_manager
