// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/footprints/internal/fpop_service_impl.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "components/signin/public/base/oauth_consumer_id.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/google_api_keys.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace contextual_search {

namespace {

const char kFpopGetFacsUrl[] =
    "https://footprints-pa.googleapis.com/v1/get_facs";
const char kFpopUpdateFacsUrl[] =
    "https://footprints-pa.googleapis.com/v1/update_activity_controls_settings";

constexpr size_t kMaxResponseSize = 1024 * 1024;  // 1 MB

const net::NetworkTrafficAnnotationTag kFpopTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("contextual_search_fpop_service", R"(
      semantics {
        sender: "My Activity Chrome Feature"
        description: "Chrome feature that retrieves or updates the status of "
                     "a setting in My Activity."
        trigger: "A feature in Chrome requests user consent status in My "
                 "Activity. For example, when a user interacts with the "
                 "'Add from Drive' (AI Mode Drive integration) drive picker "
                 "in Chrome from Realbox, Omnibox, or Cobrowse entrypoints, "
                 "Chrome checks if the user has accepted the required "
                 "disclaimer."
        data: "OAuth token and the My Activity setting ID. Different IDs "
              "will be used to check different activity control settings."
        destination: GOOGLE_OWNED_SERVICE
        user_data {
          type: ACCESS_TOKEN
        }
        last_reviewed: "2026-06-03"
        internal {
          contacts {
            email: "adamyao@google.com"
          }
        }
      }
      policy {
        cookies_allowed: NO
        setting:
          "This feature cannot be disabled in settings; requests are only "
          "triggered by explicit user actions within Chrome features that "
          "trigger the My Activity API."
        policy_exception_justification:
          "This request is only triggered by direct user interactions with "
          "the My Activity API, and does not require an enterprise policy."
      })");

}  // namespace

// static
std::unique_ptr<FpopService> FpopService::Create(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  return std::make_unique<FpopServiceImpl>(identity_manager,
                                           url_loader_factory);
}

FpopServiceImpl::FpopServiceImpl(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : identity_manager_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)) {
  CHECK(identity_manager_);
}

FpopServiceImpl::~FpopServiceImpl() = default;

void FpopServiceImpl::GetFacs(
    const footprints::oneplatform::GetFacsRequest& request,
    base::OnceCallback<void(
        bool success,
        const footprints::oneplatform::GetFacsResponse& response)> callback) {
  std::string request_body;
  if (!request.SerializeToString(&request_body)) {
    std::move(callback).Run(/*success=*/false,
                            footprints::oneplatform::GetFacsResponse());
    return;
  }

  RequestAccessToken(base::BindOnce(
      &FpopServiceImpl::SendRequest, weak_ptr_factory_.GetWeakPtr(),
      kFpopGetFacsUrl, request_body,
      base::BindOnce(&FpopServiceImpl::OnGetFacsResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback))));
}

void FpopServiceImpl::OnGetFacsResponse(
    base::OnceCallback<
        void(bool, const footprints::oneplatform::GetFacsResponse&)> callback,
    bool success,
    const std::string& response_body) {
  footprints::oneplatform::GetFacsResponse response;
  bool parse_success = response.ParseFromString(response_body);

  if (!success || !parse_success) {
    std::move(callback).Run(
        /*success=*/false, footprints::oneplatform::GetFacsResponse());
    return;
  }

  std::move(callback).Run(/*success=*/true, response);
}

void FpopServiceImpl::UpdateActivityControlsSettings(
    const footprints::oneplatform::UpdateActivityControlsSettingsRequest&
        request,
    base::OnceCallback<void(
        bool success,
        const footprints::oneplatform::UpdateActivityControlsSettingsResponse&
            response)> callback) {
  std::string request_body;
  if (!request.SerializeToString(&request_body)) {
    std::move(callback).Run(
        /*success=*/false,
        footprints::oneplatform::UpdateActivityControlsSettingsResponse());
    return;
  }

  RequestAccessToken(base::BindOnce(
      &FpopServiceImpl::SendRequest, weak_ptr_factory_.GetWeakPtr(),
      kFpopUpdateFacsUrl, request_body,
      base::BindOnce(&FpopServiceImpl::OnUpdateActivityControlsSettingsResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback))));
}

void FpopServiceImpl::OnUpdateActivityControlsSettingsResponse(
    base::OnceCallback<void(
        bool,
        const footprints::oneplatform::UpdateActivityControlsSettingsResponse&)>
        callback,
    bool success,
    const std::string& response_body) {
  footprints::oneplatform::UpdateActivityControlsSettingsResponse response;
  bool parse_success = response.ParseFromString(response_body);

  if (!success || !parse_success) {
    std::move(callback).Run(
        /*success=*/false,
        footprints::oneplatform::UpdateActivityControlsSettingsResponse());
    return;
  }

  std::move(callback).Run(/*success=*/true, response);
}

void FpopServiceImpl::RequestAccessToken(
    base::OnceCallback<void(const std::string& access_token)> callback) {
  if (access_token_fetcher_) {
    queued_token_callbacks_.push_back(std::move(callback));
    return;
  }

  access_token_fetcher_ = identity_manager_->CreateAccessTokenFetcherForAccount(
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
      signin::OAuthConsumerId::kFpopService,
      base::BindOnce(&FpopServiceImpl::OnAccessTokenFetched,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      signin::AccessTokenFetcher::Mode::kImmediate);
}

void FpopServiceImpl::OnAccessTokenFetched(
    base::OnceCallback<void(const std::string& access_token)> callback,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  access_token_fetcher_.reset();

  if (error.state() != GoogleServiceAuthError::NONE) {
    std::move(callback).Run(std::string());

    std::vector<base::OnceCallback<void(const std::string&)>> callbacks;
    callbacks.swap(queued_token_callbacks_);
    for (auto& cb : callbacks) {
      std::move(cb).Run(std::string());
    }
    return;
  }

  std::string token = access_token_info.token;

  std::move(callback).Run(token);

  std::vector<base::OnceCallback<void(const std::string&)>> callbacks;
  callbacks.swap(queued_token_callbacks_);
  for (auto& cb : callbacks) {
    std::move(cb).Run(token);
  }
}

void FpopServiceImpl::SendRequest(
    const std::string& url,
    const std::string& request_body,
    base::OnceCallback<void(bool success, const std::string& response_body)>
        callback,
    const std::string& access_token) {
  if (access_token.empty()) {
    std::move(callback).Run(/*success=*/false, /*response_body=*/std::string());
    return;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(url);
  resource_request->method = "POST";
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;

  resource_request->headers.SetHeader("Authorization",
                                      "Bearer " + access_token);
  resource_request->headers.SetHeader("X-Goog-Api-Key",
                                      google_apis::GetAPIKey());
  resource_request->headers.SetHeader("Accept", "application/x-protobuf");

  auto url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), kFpopTrafficAnnotation);
  url_loader->AttachStringForUpload(request_body, "application/x-protobuf");
  url_loader->SetAllowHttpErrorResults(true);

  auto* raw_url_loader = url_loader.get();
  raw_url_loader->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(
          [](std::unique_ptr<network::SimpleURLLoader> loader,
             base::OnceCallback<void(bool, const std::string&)> callback,
             std::optional<std::string> response_body) {
            int status_code = -1;
            if (loader->ResponseInfo() && loader->ResponseInfo()->headers) {
              status_code = loader->ResponseInfo()->headers->response_code();
            }
            std::string body_str = response_body.value_or(std::string());
            bool success = loader->NetError() == net::OK &&
                           status_code == net::HTTP_OK &&
                           response_body.has_value();

            std::move(callback).Run(/*success=*/success, body_str);
          },
          std::move(url_loader), std::move(callback)),
      kMaxResponseSize);
}

}  // namespace contextual_search
