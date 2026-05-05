// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_verification_tokens/common/private_verification_tokens_fetcher.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "url/gurl.h"

namespace private_verification_tokens {

namespace {

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation(
        "private_verification_tokens_service_get_tokens",
        R"(
    semantics {
      sender: "Private Verification Tokens Service Client"
      description:
        "Request to a registered issuer to obtain Private Verification Tokens."
      trigger:
        "User visits registered page."
      data:
        "Serialized token request, key id and version"
      destination: WEBSITE
      internal {
        contacts {
          email: "private-verification-tokens-chrome-team@google.com"
        }
      }
      user_data {
        type: CREDENTIALS
      }
      last_reviewed: "2026-04-23"
    }
    policy {
      cookies_allowed: YES
      cookies_store: "user"
      setting: "Disable `Auto-verify` at chrome://settings/content/autoVerify."
      policy_exception_justification: "Not implemented."
    }
    comments:
      ""
    )");

// Accept and Content-Type headers as defined in
// https://www.rfc-editor.org/rfc/rfc9578.html#section-5.1
constexpr char kAccept[] = "application/private-token-response";
constexpr char kContentType[] = "application/private-token-request";
constexpr base::TimeDelta kFetchTimeout = base::Minutes(1);
// Response max is set based on ATHM token size and max batch size of
// 20, Ns=32 and Ne=33.
constexpr size_t kResponseMaxBodySize = 2 * 1024;

network::ResourceRequest CreateFetchRequest(GURL issue_url) {
  network::ResourceRequest resource_request;
  resource_request.url = std::move(issue_url);
  resource_request.method = net::HttpRequestHeaders::kPostMethod;
  resource_request.credentials_mode = network::mojom::CredentialsMode::kInclude;
  resource_request.headers.SetHeader(net::HttpRequestHeaders::kAccept, kAccept);
  return resource_request;
}

}  // namespace

// static
std::unique_ptr<PrivateVerificationTokensFetcher>
PrivateVerificationTokensFetcher::Create(
    GURL issue_url,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_url_loader_factory) {
  if (!pending_url_loader_factory) {
    return nullptr;
  }
  if (!issue_url.is_valid()) {
    return nullptr;
  }
  return base::WrapUnique<PrivateVerificationTokensFetcher>(
      new PrivateVerificationTokensFetcher(
          std::move(issue_url), std::move(pending_url_loader_factory)));
}

PrivateVerificationTokensFetcher::PrivateVerificationTokensFetcher(
    GURL issue_url,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_url_loader_factory)
    : request_(CreateFetchRequest(std::move(issue_url))),
      url_loader_factory_(network::SharedURLLoaderFactory::Create(
          std::move(pending_url_loader_factory))) {
  CHECK(url_loader_factory_);
  CHECK(request_.url.is_valid());
}

PrivateVerificationTokensFetcher::~PrivateVerificationTokensFetcher() = default;

void PrivateVerificationTokensFetcher::TryGetTokens(
    const std::string& request_body,
    TryGetTokensCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::unique_ptr<network::SimpleURLLoader> url_loader =
      network::SimpleURLLoader::Create(
          std::make_unique<network::ResourceRequest>(request_),
          kTrafficAnnotation);

  // Retry on network changes, as sometimes this occurs during browser startup.
  // A network change during DNS resolution results in a DNS error rather than
  // a network change error, so retry in those cases as well.
  url_loader->SetRetryOptions(
      2, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE |
             network::SimpleURLLoader::RETRY_ON_NAME_NOT_RESOLVED);
  url_loader->AttachStringForUpload(request_body, kContentType);
  url_loader->SetTimeoutDuration(kFetchTimeout);

  // Get pointer to url_loader before moving it.
  auto* url_loader_ptr = url_loader.get();
  url_loader_ptr->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&PrivateVerificationTokensFetcher::OnGetTokensCompleted,
                     weak_ptr_factory_.GetWeakPtr(),
                     // Including the URLLoader in the callback is vital to get
                     // error code and prevent it from going out of scope until
                     // the download is complete.
                     std::move(url_loader), std::move(callback)),
      kResponseMaxBodySize);
}

void PrivateVerificationTokensFetcher::OnGetTokensCompleted(
    std::unique_ptr<network::SimpleURLLoader> url_loader,
    TryGetTokensCallback callback,
    std::optional<std::string> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (url_loader->NetError() != net::OK) {
    std::move(callback).Run(base::unexpected(TryGetTokensResult{
        TryGetTokensError::kNetNotOk, url_loader->NetError()}));
    return;
  }
  if (!response.has_value()) {
    // url_loader->NetError() is net::OK, however, DownloadToString returned
    // null response.
    std::move(callback).Run(base::unexpected(
        TryGetTokensResult{TryGetTokensError::kNullResponse, net::OK}));
    return;
  }
  std::move(callback).Run(std::move(response.value()));
}

}  // namespace private_verification_tokens
