// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/distiller_url_fetcher.h"

#include "base/bind.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace dom_distiller {

DistillerURLFetcherFactory::DistillerURLFetcherFactory(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory) {}

DistillerURLFetcherFactory::~DistillerURLFetcherFactory() {}

DistillerURLFetcher* DistillerURLFetcherFactory::CreateDistillerURLFetcher()
    const {
  return new DistillerURLFetcher(url_loader_factory_);
}

DistillerURLFetcher::DistillerURLFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory) {}

DistillerURLFetcher::~DistillerURLFetcher() {}

void DistillerURLFetcher::FetchURL(const std::string& url,
                                   const URLFetcherCallback& callback) {
  // Don't allow a fetch if one is pending.
  DCHECK(!url_loader_);
  callback_ = callback;
  url_loader_ = CreateURLFetcher(url);
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&DistillerURLFetcher::OnURLLoadComplete,
                     base::Unretained(this)));
}

std::unique_ptr<network::SimpleURLLoader> DistillerURLFetcher::CreateURLFetcher(
    const std::string& url) {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("dom_distiller", R"(
        semantics {
          sender: "DOM Distiller"
          description:
            "Chromium provides Mobile-friendly view on Android phones when the "
            "web page contains an article, and is not mobile-friendly. If the "
            "user enters Mobile-friendly view, the main content would be "
            "extracted and reflowed in a simple layout for better readability. "
            "On iOS, apps can add URLs to the Reading List in Chromium. When "
            "opening the entries in the Reading List with no or limited "
            "network, the simple layout would be shown. DOM distiller is the "
            "backend service for Mobile-friendly view and Reading List."
          trigger:
            "When the user enters Mobile-friendly view on Android phones, or "
            "adds entries to the Reading List on iOS. Note that Reading List "
            "entries can be added from other apps."
          data:
            "URLs of the required website resources to fetch."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting: "Users can enable or disable Mobile-friendly view by "
          "toggling chrome://flags#reader-mode-heuristics in Chromium on "
          "Android."
          policy_exception_justification:
            "Not implemented, considered not useful as no content is being "
            "uploaded; this request merely downloads the resources on the web."
        })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(url);
  resource_request->method = "GET";

  auto url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  static const int kMaxRetries = 5;
  url_loader->SetRetryOptions(kMaxRetries,
                              network::SimpleURLLoader::RETRY_ON_5XX);

  return url_loader;
}

void DistillerURLFetcher::OnURLLoadComplete(
    std::unique_ptr<std::string> response_body) {
  // The loader is not needed at this point anymore.
  url_loader_.reset();

  std::string response;
  if (response_body) {
    // Only copy over the data if the request was successful. Insert
    // an empty string into the proto otherwise.
    response = std::move(*response_body);
  }
  callback_.Run(response);
}

}  // namespace dom_distiller
