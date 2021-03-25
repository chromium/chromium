// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/service/service_request_sender_impl.h"

#include "base/strings/strcat.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace {

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("autofill_service", R"(
        semantics {
          sender: "Autofill Assistant"
          description:
            "Chromium posts requests to autofill assistant server to get
            scripts for a URL."
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

void OnURLLoaderComplete(
    autofill_assistant::ServiceRequestSender::ResponseCallback callback,
    std::unique_ptr<::network::SimpleURLLoader> loader,
    std::unique_ptr<std::string> response_body) {
  std::string response_str;
  if (response_body != nullptr) {
    response_str = std::move(*response_body);
  }

  int response_code = 0;
  if (loader->ResponseInfo() && loader->ResponseInfo()->headers) {
    response_code = loader->ResponseInfo()->headers->response_code();
  }
  VLOG(3) << "Received response: status=" << response_code << ", "
          << response_str.length() << " bytes";
  std::move(callback).Run(response_code, response_str);
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
    content::BrowserContext* context,
    autofill_assistant::SimpleURLLoaderFactory* loader_factory,
    autofill_assistant::ServiceRequestSender::ResponseCallback callback) {
  auto loader =
      loader_factory->CreateLoader(std::move(request), kTrafficAnnotation);
  loader->AttachStringForUpload(request_body, "application/x-protobuffer");
#ifdef DEBUG
  loader->SetAllowHttpErrorResults(true);
#endif
  auto* const loader_ptr = loader.get();
  loader_ptr->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      content::BrowserContext::GetDefaultStoragePartition(context)
          ->GetURLLoaderFactoryForBrowserProcess()
          .get(),
      base::BindOnce(&OnURLLoaderComplete, std::move(callback),
                     std::move(loader)));
}

void SendRequestNoAuth(
    const GURL& url,
    const std::string& request_body,
    content::BrowserContext* context,
    autofill_assistant::SimpleURLLoaderFactory* loader_factory,
    const std::string& api_key,
    autofill_assistant::ServiceRequestSender::ResponseCallback callback) {
  if (callback.IsCancelled()) {
    return;
  }
  if (api_key.empty()) {
    LOG(ERROR) << "no api key provided";
    std::move(callback).Run(net::HTTP_UNAUTHORIZED, std::string());
    return;
  }

  std::string query_str = base::StrCat({"key=", api_key});
  // query_str must remain valid until ReplaceComponents() has returned.
  GURL::Replacements add_key;
  add_key.SetQueryStr(query_str);
  GURL modified_url = url.ReplaceComponents(add_key);

  VLOG(2) << "Sending request with api key to backend";
  SendRequestImpl(CreateResourceRequest(modified_url), request_body, context,
                  loader_factory, std::move(callback));
}

}  // namespace

namespace autofill_assistant {

ServiceRequestSenderImpl::ServiceRequestSenderImpl(
    content::BrowserContext* context,
    AccessTokenFetcher* access_token_fetcher,
    std::unique_ptr<SimpleURLLoaderFactory> loader_factory,
    const std::string& api_key,
    bool auth_enabled,
    bool disable_auth_if_no_access_token)
    : context_(context),
      access_token_fetcher_(access_token_fetcher),
      loader_factory_(std::move(loader_factory)),
      api_key_(api_key),
      auth_enabled_(auth_enabled),
      disable_auth_if_no_access_token_(disable_auth_if_no_access_token) {
  DCHECK(!auth_enabled || access_token_fetcher != nullptr);
  DCHECK(auth_enabled || !api_key.empty());
}
ServiceRequestSenderImpl::~ServiceRequestSenderImpl() = default;

void ServiceRequestSenderImpl::SendRequest(const GURL& url,
                                           const std::string& request_body,
                                           ResponseCallback callback) {
  if (auth_enabled_ && access_token_fetcher_ == nullptr) {
    LOG(ERROR) << "auth requested, but no access token fetcher provided";
    std::move(callback).Run(net::HTTP_UNAUTHORIZED, std::string());
    return;
  }
  if (auth_enabled_) {
    access_token_fetcher_->FetchAccessToken(
        base::BindOnce(&ServiceRequestSenderImpl::OnFetchAccessToken,
                       weak_ptr_factory_.GetWeakPtr(), url, request_body,
                       std::move(callback)));
    return;
  }

  SendRequestNoAuth(url, request_body, context_, loader_factory_.get(),
                    api_key_, std::move(callback));
}

void ServiceRequestSenderImpl::OnFetchAccessToken(
    GURL url,
    std::string request_body,
    ResponseCallback callback,
    bool access_token_fetched,
    const std::string& access_token) {
  if (!access_token_fetched || access_token.empty()) {
    if (disable_auth_if_no_access_token_) {
      // Give up on authentication for this run. Without access token, requests
      // might be successful or rejected, depending on the server configuration.
      auth_enabled_ = false;
      VLOG(1) << "No access token, falling back to api key";
      SendRequestNoAuth(url, request_body, context_, loader_factory_.get(),
                        api_key_, std::move(callback));
      return;
    }
    VLOG(1) << "No access token, but disable_auth_if_no_access_token not set";
    std::move(callback).Run(net::HTTP_UNAUTHORIZED, std::string());
    return;
  }

  SendRequestAuth(url, request_body, access_token, std::move(callback));
}

void ServiceRequestSenderImpl::SendRequestAuth(const GURL& url,
                                               const std::string& request_body,
                                               const std::string& access_token,
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
                              request_body, std::move(callback));
  }
  VLOG(2) << "Sending request with access token to backend";
  SendRequestImpl(std::move(resource_request), request_body, context_,
                  loader_factory_.get(), std::move(callback));
}

void ServiceRequestSenderImpl::RetryIfUnauthorized(
    const GURL& url,
    const std::string& access_token,
    const std::string& request_body,
    ResponseCallback callback,
    int http_status,
    const std::string& response) {
  // On first UNAUTHORIZED error, invalidate access token and try again.
  if (auth_enabled_ && http_status == net::HTTP_UNAUTHORIZED) {
    VLOG(1) << "Request with access token returned with 401 UNAUTHORIZED, "
               "fetching a fresh access token and trying again";
    DCHECK(!retried_with_fresh_access_token_);
    retried_with_fresh_access_token_ = true;
    access_token_fetcher_->InvalidateAccessToken(access_token);
    SendRequest(url, request_body, std::move(callback));
    return;
  }
  std::move(callback).Run(http_status, response);
}

}  // namespace autofill_assistant
