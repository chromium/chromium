// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_url_fetcher.h"

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/variations/net/variations_http_headers.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace translate {

namespace {

// Retry parameter for fetching.
const int kMaxRetry = 16;

}  // namespace

TranslateURLFetcher::TranslateURLFetcher()
    : state_(IDLE), retry_count_(0), max_retry_on_5xx_(0) {}

TranslateURLFetcher::~TranslateURLFetcher() {}

bool TranslateURLFetcher::Request(const GURL& url,
                                  TranslateURLFetcher::Callback callback,
                                  bool is_incognito) {
  // This function is not supposed to be called if the previous operation is not
  // finished.
  if (state_ == REQUESTING) {
    NOTREACHED();
    return false;
  }

  if (retry_count_ >= kMaxRetry)
    return false;
  retry_count_++;

  state_ = REQUESTING;
  url_ = url;
  callback_ = std::move(callback);

  // If the TranslateDownloadManager's request context getter is nullptr then
  // shutdown is in progress. Abort the request, which can't proceed with a
  // null url_loader_factory.
  network::mojom::URLLoaderFactory* url_loader_factory =
      TranslateDownloadManager::GetInstance()->url_loader_factory().get();
  if (!url_loader_factory)
    return false;

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("translate_url_fetcher", R"(
        semantics {
          sender: "Translate"
          description:
            "Chrome can provide translations for the web sites visited by the "
            "user. If this feature is enabled, Chrome sends network requests "
            "to download the list of supported languages and a library to "
            "perform translations."
          trigger:
            "When Chrome starts, it downloads the list of supported languages "
            "for translation. The first time Chrome decides to offer "
            "translation of a web site, it triggers a popup to ask "
            "if user wants a translation and if user approves, "
            "translation library is downloaded. The library is cached for a "
            "day and is not fetched if it is available and fresh."
          data:
            "Current locale is sent to fetch the list of supported languages. "
            "Translation library that is obtained via this interface would "
            "perform actual translation, and it will send words and phrases in "
            "the site to the server to translate it, but this request doesn't "
            "send any words."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can enable/disable this feature by toggling 'Offer to "
            "translate pages that aren't in a language you read.' in Chrome "
            "settings under Languages. The list of supported languages is "
            "downloaded regardless of the settings."
          chrome_policy {
            TranslateEnabled {
              TranslateEnabled: false
            }
          }
        })");

  // Create and initialize URL loader.
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url_;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  if (!extra_request_header_.empty())
    resource_request->headers.AddHeaderFromString(extra_request_header_);

  simple_loader_ =
      variations::CreateSimpleURLLoaderWithVariationsHeaderUnknownSignedIn(
          std::move(resource_request),
          is_incognito ? variations::InIncognito::kYes
                       : variations::InIncognito::kNo,
          traffic_annotation);
  // Set retry parameter for HTTP status code 5xx. This doesn't work against
  // 106 (net::ERR_INTERNET_DISCONNECTED) and so on.
  // TranslateLanguageList handles network status, and implements retry.
  if (max_retry_on_5xx_) {
    simple_loader_->SetRetryOptions(
        max_retry_on_5xx_, network::SimpleURLLoader::RetryMode::RETRY_ON_5XX);
  }

  simple_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory,
      base::BindOnce(&TranslateURLFetcher::OnSimpleLoaderComplete,
                     base::Unretained(this)));
  return true;
}

void TranslateURLFetcher::OnSimpleLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  std::string data;
  if (response_body) {
    DCHECK_EQ(net::OK, simple_loader_->NetError());
    data = std::move(*response_body);
    state_ = COMPLETED;
  } else {
    state_ = FAILED;
  }

  simple_loader_.reset();

  std::move(callback_).Run(state_ == COMPLETED, data);
}

}  // namespace translate
