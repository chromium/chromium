// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/hashprefix_realtime/ohttp_key_service.h"

#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

constexpr base::TimeDelta kKeyFetchTimeout = base::Seconds(3);
// TODO(crbug.com/1407283): Update the endpoint when it is finalized.
constexpr char kKeyFetchServerUrl[] =
    "https://safebrowsingohttpgateway.googleapis.com/key";

constexpr net::NetworkTrafficAnnotationTag kOhttpKeyTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("safe_browsing_ohttp_key_fetch",
                                        R"(
  semantics {
    sender: "Safe Browsing"
    description:
      "Get the Oblivious HTTP key for hash real time URL check."
    trigger:
      "Periodically fetching the key once every few hours or fetching the key "
      "during hash real time URL check if there is no key available."
    data:
        "A simple GET HTTP request. No user data is included."
    destination: GOOGLE_OWNED_SERVICE
    internal {
      contacts {
        email: "xinghuilu@chromium.org"
      }
      contacts {
        email: "chrome-counter-abuse-alerts@google.com"
      }
    }
    user_data {
      type: NONE
    }
    last_reviewed: "2023-03-06"
  }
  policy {
    cookies_allowed: NO
    setting:
      "Users can disable this feature by unselecting 'Standard protection' "
      "in Chromium settings under Security. The feature is enabled by default."
    chrome_policy {
      SafeBrowsingProtectionLevel {
        policy_options {mode: MANDATORY}
        SafeBrowsingProtectionLevel: 0
      }
    }
  }
  comments:
      "SafeBrowsingProtectionLevel value of 0 or 2 disables fetching this "
      "OHTTP key. A value of 1 enables the feature. The feature is enabled by "
      "default."
  )");

}  // namespace

namespace safe_browsing {

OhttpKeyService::OhttpKeyService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory) {}

OhttpKeyService::~OhttpKeyService() = default;

void OhttpKeyService::GetOhttpKey(Callback callback) {
  pending_callbacks_.AddUnsafe(std::move(callback));
  // If url_loader_ is not null, that means a request is already in progress.
  // Will notify the callback when it is completed.
  if (url_loader_) {
    return;
  }
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(kKeyFetchServerUrl);
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 kOhttpKeyTrafficAnnotation);
  url_loader_->SetTimeoutDuration(kKeyFetchTimeout);
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&OhttpKeyService::OnURLLoaderComplete,
                     weak_factory_.GetWeakPtr()));
}

void OhttpKeyService::OnURLLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  // TODO(crbug.com/1407283): Log net error and response code.
  DCHECK(url_loader_);
  int response_code = 0;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
    response_code = url_loader_->ResponseInfo()->headers->response_code();
  }
  bool is_key_fetch_successful = response_body &&
                                 url_loader_->NetError() == net::OK &&
                                 response_code == net::HTTP_OK;

  url_loader_.reset();
  pending_callbacks_.Notify(is_key_fetch_successful
                                ? absl::optional<std::string>(*response_body)
                                : absl::nullopt);
}

void OhttpKeyService::Shutdown() {
  url_loader_.reset();
  pending_callbacks_.Notify(absl::nullopt);
}

}  // namespace safe_browsing
