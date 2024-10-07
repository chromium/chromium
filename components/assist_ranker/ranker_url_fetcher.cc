// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/assist_ranker/ranker_url_fetcher.h"

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace assist_ranker {

namespace {

// Retry parameter for fetching.
const int kMaxRetry = 16;

}  // namespace

RankerURLFetcher::RankerURLFetcher()
    : state_(IDLE), retry_count_(0), max_retry_on_5xx_(0) {}

RankerURLFetcher::~RankerURLFetcher() = default;

bool RankerURLFetcher::Request(
    const GURL& url,
    RankerURLFetcher::Callback callback,
    network::mojom::URLLoaderFactory* url_loader_factory) {
  // This function is not supposed to be called if the previous operation is not
  // finished.
  if (state_ == REQUESTING) {
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  if (retry_count_ >= kMaxRetry)
    return false;
  retry_count_++;

  state_ = REQUESTING;
  url_ = url;
  callback_ = std::move(callback);

  if (url_loader_factory == nullptr)
    return false;

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("ranker_url_fetcher", R"(
        semantics {
          sender: "AssistRanker"
          description:
            "Chrome can provide a better UI experience by using machine "
            "learning models to determine if we should show you or not an "
            "assist prompt. For instance, Chrome may use features such as "
            "the detected language of the current page and the past "
            "interaction with the TransalteUI to decide whether or not we "
            "should offer you to translate this page. Google returns "
            "trained machine learning models that will be used to take "
            "such decision."
          trigger:
            "At startup."
          data:
            "Path to a model. No user data is included."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "NA"
          policy_exception_justification:
            "Not implemented, considered not necessary as no user data is sent."
        })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url_;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  if (max_retry_on_5xx_ > 0) {
    simple_url_loader_->SetRetryOptions(max_retry_on_5xx_,
                                        network::SimpleURLLoader::RETRY_ON_5XX);
  }
  simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory,
      base::BindOnce(&RankerURLFetcher::OnSimpleLoaderComplete,
                     base::Unretained(this)));

  return true;
}

void RankerURLFetcher::OnSimpleLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  std::string data;
  if (response_body) {
    state_ = COMPLETED;
    data = std::move(*response_body);
  } else {
    state_ = FAILED;
  }
  simple_url_loader_.reset();
  std::move(callback_).Run(state_ == COMPLETED, data);
}

}  // namespace assist_ranker
