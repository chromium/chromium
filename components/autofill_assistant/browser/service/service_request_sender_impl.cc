// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/service/service_request_sender_impl.h"

#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/service/cup.h"
#include "components/autofill_assistant/browser/service/cup_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("autofill_service", R"(
        semantics {
          sender: "Autofill Assistant"
          description:
            "Chromium posts requests to autofill assistant server to get "
            "scripts for a URL."
          trigger:
            "Matching URL."
          data: "None."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature can be disabled in settings."
          policy_exception_justification: "Not implemented."
        })");

// We want to retry the "get user data" call in case it fails as users cannot
// continue the flow without proper data.
constexpr int kMaxRetriesGetUserData = 2;

void OnURLLoaderComplete(
    autofill_assistant::ServiceRequestSender::ResponseCallback callback,
    std::unique_ptr<::network::SimpleURLLoader> loader,
    int max_retries,
    std::unique_ptr<std::string> response_body) {
  std::string response_str;
  if (response_body != nullptr) {
    response_str = std::move(*response_body);
  }

  int response_code = 0;
  if (loader->ResponseInfo() && loader->ResponseInfo()->headers) {
    response_code = loader->ResponseInfo()->headers->response_code();
  }

  autofill_assistant::ServiceRequestSender::ResponseInfo response_info;
  if (loader->CompletionStatus().has_value()) {
    response_info.encoded_body_length =
        loader->CompletionStatus()->encoded_body_length;
  }

  VLOG(3) << "Received response: status=" << response_code << ", "
          << "encoded: " << response_info.encoded_body_length << " bytes, "
          << "decoded: " << response_str.length() << " bytes";

  if (max_retries > 0) {
    autofill_assistant::Metrics::RecordServiceRequestRetryCount(
        loader->GetNumRetries(), response_code == net::HTTP_OK);
  }
  std::move(callback).Run(response_code, response_str, response_info);
}

std::unique_ptr<::network::ResourceRequest> CreateResourceRequest(GURL url) {
  auto resource_request = std::make_unique<::network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = "POST";
  resource_request->redirect_mode = ::network::mojom::RedirectMode::kError;
  resource_request->credentials_mode = ::network::mojom::CredentialsMode::kOmit;
  return resource_request;
}

void SendRequestImpl(
    std::unique_ptr<::network::ResourceRequest> request,
    const std::string& request_body,
    int max_retries,
    content::BrowserContext* context,
    autofill_assistant::SimpleURLLoaderFactory* loader_factory,
    autofill_assistant::ServiceRequestSender::ResponseCallback callback) {
  auto loader =
      loader_factory->CreateLoader(std::move(request), kTrafficAnnotation);
  if (max_retries > 0) {
    loader->SetRetryOptions(
        max_retries, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE |
                         network::SimpleURLLoader::RETRY_ON_NAME_NOT_RESOLVED |
                         network::SimpleURLLoader::RETRY_ON_5XX);
  }
  loader->AttachStringForUpload(request_body, "application/x-protobuffer");
#ifndef NDEBUG
  loader->SetAllowHttpErrorResults(true);
#endif
  auto* const loader_ptr = loader.get();
  loader_ptr->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      context->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess()
          .get(),
      base::BindOnce(&OnURLLoaderComplete, std::move(callback),
                     std::move(loader), max_retries));
}

void SendRequestNoAuth(
    const GURL& url,
    const std::string& request_body,
    int max_retries,
    content::BrowserContext* context,
    autofill_assistant::SimpleURLLoaderFactory* loader_factory,
    const std::string& api_key,
    autofill_assistant::ServiceRequestSender::ResponseCallback callback) {
  if (callback.IsCancelled()) {
    return;
  }
  if (api_key.empty()) {
    LOG(ERROR) << "no api key provided";
    std::move(callback).Run(net::HTTP_UNAUTHORIZED, std::string(),
                            /* response_info = */ {});
    return;
  }

  std::string query_str = base::StrCat({"key=", api_key});
  // query_str must remain valid until ReplaceComponents() has returned.
  GURL::Replacements add_key;
  add_key.SetQueryStr(query_str);
  GURL modified_url = url.ReplaceComponents(add_key);

#ifdef NDEBUG
  VLOG(2) << "Sending request with api key to backend";
#else
  VLOG(2) << "Sending request with api key to backend: " << modified_url;
#endif
  SendRequestImpl(CreateResourceRequest(modified_url), request_body,
                  max_retries, context, loader_factory, std::move(callback));
}

void MaybeVerifyCupResponse(
    std::unique_ptr<autofill_assistant::cup::CUP> cup,
    autofill_assistant::RpcType rpc_type,
    autofill_assistant::ServiceRequestSender::ResponseCallback callback,
    int http_status,
    const std::string& response,
    const autofill_assistant::ServiceRequestSender::ResponseInfo&
        response_info) {
  if (!autofill_assistant::cup::IsRpcTypeSupported(rpc_type)) {
    return std::move(callback).Run(http_status, response, response_info);
  }
  if (http_status != net::HTTP_OK) {
    autofill_assistant::Metrics::RecordCupRpcVerificationEvent(
        autofill_assistant::Metrics::CupRpcVerificationEvent::HTTP_FAILED);
    return std::move(callback).Run(http_status, std::string(),
                                   /* response_info = */ {});
  }
  if (!autofill_assistant::cup::ShouldSignRequests(rpc_type)) {
    autofill_assistant::Metrics::RecordCupRpcVerificationEvent(
        autofill_assistant::Metrics::CupRpcVerificationEvent::SIGNING_DISABLED);
    return std::move(callback).Run(http_status, response, response_info);
  }
  if (!autofill_assistant::cup::ShouldVerifyResponses(rpc_type)) {
    autofill_assistant::Metrics::RecordCupRpcVerificationEvent(
        autofill_assistant::Metrics::CupRpcVerificationEvent::
            VERIFICATION_DISABLED);
    return std::move(callback).Run(http_status, response, response_info);
  }

  absl::optional<std::string> unpacked_response = cup->UnpackResponse(response);
  if (!unpacked_response) {
    LOG(ERROR) << "Failed to unpack or verify a response.";
    return std::move(callback).Run(net::HTTP_UNAUTHORIZED, std::string(),
                                   /* response_info = */ {});
  }

  return std::move(callback).Run(http_status, *unpacked_response,
                                 response_info);
}

}  // namespace

namespace autofill_assistant {

ServiceRequestSenderImpl::ServiceRequestSenderImpl(
    content::BrowserContext* context,
    AccessTokenFetcher* access_token_fetcher,
    std::unique_ptr<cup::CUPFactory> cup_factory,
    std::unique_ptr<SimpleURLLoaderFactory> loader_factory,
    const std::string& api_key)
    : context_(context),
      access_token_fetcher_(access_token_fetcher),
      cup_factory_(std::move(cup_factory)),
      loader_factory_(std::move(loader_factory)),
      api_key_(api_key) {}
ServiceRequestSenderImpl::~ServiceRequestSenderImpl() = default;

void ServiceRequestSenderImpl::SendRequest(
    const GURL& url,
    const std::string& request_body,
    ServiceRequestSender::AuthMode auth_mode,
    ResponseCallback callback,
    RpcType rpc_type) {
  int max_retries = 0;
  if (rpc_type == RpcType::GET_USER_DATA) {
    max_retries = kMaxRetriesGetUserData;
  }

  if (!cup::IsRpcTypeSupported(rpc_type) || disable_rpc_signing_) {
    InternalSendRequest(url, request_body, auth_mode, max_retries,
                        std::move(callback));
    return;
  }

  std::unique_ptr<cup::CUP> cup = cup_factory_->CreateInstance(rpc_type);
  std::string maybe_signed_request = request_body;
  if (cup::ShouldSignRequests(rpc_type)) {
    maybe_signed_request = cup->PackAndSignRequest(request_body);
  }

  auto wrapped_callback = base::BindOnce(
      &MaybeVerifyCupResponse, std::move(cup), rpc_type, std::move(callback));

  InternalSendRequest(url, maybe_signed_request, auth_mode, max_retries,
                      std::move(wrapped_callback));
}

void ServiceRequestSenderImpl::InternalSendRequest(
    const GURL& url,
    const std::string& request_body,
    ServiceRequestSender::AuthMode auth_mode,
    int max_retries,
    ResponseCallback callback) {
  if (OAuthEnabled(auth_mode) && access_token_fetcher_ == nullptr) {
    LOG(ERROR) << "auth requested, but no access token fetcher provided";
    std::move(callback).Run(net::HTTP_UNAUTHORIZED, std::string(),
                            /* response_info = */ {});
    return;
  }
  if (OAuthEnabled(auth_mode)) {
    access_token_fetcher_->FetchAccessToken(
        base::BindOnce(&ServiceRequestSenderImpl::OnFetchAccessToken,
                       weak_ptr_factory_.GetWeakPtr(), url, request_body,
                       auth_mode, max_retries, std::move(callback)));
    return;
  }

  DCHECK(!api_key_.empty());
  SendRequestNoAuth(url, request_body, max_retries, context_,
                    loader_factory_.get(), api_key_, std::move(callback));
}

void ServiceRequestSenderImpl::OnFetchAccessToken(
    GURL url,
    std::string request_body,
    ServiceRequestSender::AuthMode auth_mode,
    int max_retries,
    ResponseCallback callback,
    bool access_token_fetched,
    const std::string& access_token) {
  if (!access_token_fetched || access_token.empty()) {
    if (auth_mode != ServiceRequestSender::AuthMode::OAUTH_STRICT) {
      // Give up on authentication for this run. Without access token, requests
      // might be successful or rejected, depending on the server configuration.
      failed_to_fetch_oauth_token_ = true;
      VLOG(1) << "No access token, falling back to api key";
      SendRequestNoAuth(url, request_body, max_retries, context_,
                        loader_factory_.get(), api_key_, std::move(callback));
      return;
    }
    VLOG(1) << "No access token but authentication is required.";
    std::move(callback).Run(net::HTTP_UNAUTHORIZED, std::string(),
                            /* response_info = */ {});
    return;
  }

  failed_to_fetch_oauth_token_ = false;
  SendRequestAuth(url, request_body, access_token, auth_mode, max_retries,
                  std::move(callback));
}

void ServiceRequestSenderImpl::SendRequestAuth(
    const GURL& url,
    const std::string& request_body,
    const std::string& access_token,
    ServiceRequestSender::AuthMode auth_mode,
    int max_retries,
    ResponseCallback callback) {
  if (callback.IsCancelled()) {
    return;
  }
  auto resource_request = CreateResourceRequest(url);
  resource_request->headers.SetHeader("Authorization",
                                      base::StrCat({"Bearer ", access_token}));

  if (!retried_with_fresh_access_token_) {
    callback = base::BindOnce(&ServiceRequestSenderImpl::RetryIfUnauthorized,
                              weak_ptr_factory_.GetWeakPtr(), url, access_token,
                              request_body, auth_mode, max_retries,
                              std::move(callback));
  }
#ifdef NDEBUG
  VLOG(2) << "Sending request with access token to backend";
#else
  VLOG(2) << "Sending request with access token to backend: " << url;
#endif
  SendRequestImpl(std::move(resource_request), request_body, max_retries,
                  context_, loader_factory_.get(), std::move(callback));
}

void ServiceRequestSenderImpl::RetryIfUnauthorized(
    const GURL& url,
    const std::string& access_token,
    const std::string& request_body,
    ServiceRequestSender::AuthMode auth_mode,
    int max_retries,
    ResponseCallback callback,
    int http_status,
    const std::string& response,
    const ResponseInfo& response_info) {
  // On first UNAUTHORIZED error, invalidate access token and try again.
  if (OAuthEnabled(auth_mode) && http_status == net::HTTP_UNAUTHORIZED) {
    VLOG(1) << "Request with access token returned with 401 UNAUTHORIZED, "
               "fetching a fresh access token and trying again";
    DCHECK(!retried_with_fresh_access_token_);
    retried_with_fresh_access_token_ = true;
    access_token_fetcher_->InvalidateAccessToken(access_token);
    InternalSendRequest(url, request_body, auth_mode, max_retries,
                        std::move(callback));
    return;
  }
  std::move(callback).Run(http_status, response, response_info);
}

bool ServiceRequestSenderImpl::OAuthEnabled(
    ServiceRequestSender::AuthMode auth_mode) {
  return auth_mode == ServiceRequestSender::AuthMode::OAUTH_STRICT ||
         (auth_mode ==
              ServiceRequestSender::AuthMode::OAUTH_WITH_API_KEY_FALLBACK &&
          !failed_to_fetch_oauth_token_);
}

void ServiceRequestSenderImpl::SetDisableRpcSigning(bool disable_rpc_signing) {
  disable_rpc_signing_ = disable_rpc_signing;
}

}  // namespace autofill_assistant
