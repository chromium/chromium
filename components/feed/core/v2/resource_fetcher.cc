// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/resource_fetcher.h"

#include "components/feed/core/v2/metrics_reporter.h"
#include "components/feed/core/v2/public/types.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace feed {

namespace {

bool IsSupportedHttpMethod(const std::string& method) {
  return method == net::HttpRequestHeaders::kGetMethod ||
         method == net::HttpRequestHeaders::kPostMethod ||
         method == net::HttpRequestHeaders::kHeadMethod;
}

}  // namespace

ResourceFetcher::ResourceFetcher(
    scoped_refptr<::network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory) {}

ResourceFetcher::~ResourceFetcher() = default;

void ResourceFetcher::Fetch(
    const GURL& url,
    const std::string& method,
    const std::vector<std::string>& header_names_and_values,
    const std::string& post_data,
    ResourceCallback callback) {
  int status_code = net::OK;
  if (!IsSupportedHttpMethod(method)) {
    status_code = net::ERR_METHOD_NOT_SUPPORTED;
  } else if ((header_names_and_values.size() % 2) > 0) {
    status_code = net::ERR_INVALID_ARGUMENT;
  } else if (!post_data.empty() &&
             method != net::HttpRequestHeaders::kPostMethod) {
    status_code = net::ERR_INVALID_ARGUMENT;
  }
  if (status_code != net::OK) {
    NetworkResponse response;
    response.status_code = status_code;
    std::move(callback).Run(std::move(response));
    return;
  }

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("interest_feedv2_resource_send", R"(
        semantics {
          sender: "Feed Library"
          description:
            "Download resource data, like live sports or weather, that can be "
            "used to provide additional information for the articles in the "
            "feed."
          trigger: "Triggered when viewing the feed on the NTP."
          user_data {
            type: NONE
          }
          data:
            "The data to identify and render the additioal resource, like "
            "sports game ID and language."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              email: "chrome-feed-fundamentals@google.com"
            }
          }
          last_reviewed: "2023-08-21"
        }
        policy {
          cookies_allowed: NO
          setting: "This can be disabled from the New Tab Page by collapsing "
          "the articles section."
          chrome_policy {
            NTPContentSuggestionsEnabled {
              policy_options {mode: MANDATORY}
              NTPContentSuggestionsEnabled: false
            }
          }
        })");
  auto resource_request = std::make_unique<::network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = method;
  if (!header_names_and_values.empty()) {
    for (size_t i = 0; i < header_names_and_values.size(); i += 2) {
      resource_request->headers.SetHeader(header_names_and_values[i],
                                          header_names_and_values[i + 1]);
    }
  }

  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  auto simple_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  auto* const simple_loader_ptr = simple_loader.get();

  if (!post_data.empty()) {
    simple_loader->AttachStringForUpload(post_data);
  }

  simple_loader_ptr->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&ResourceFetcher::OnFetchComplete,
                     weak_factory_.GetWeakPtr(), std::move(simple_loader),
                     std::move(callback)),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

void ResourceFetcher::OnFetchComplete(
    std::unique_ptr<network::SimpleURLLoader> url_loader,
    ResourceCallback callback,
    std::unique_ptr<std::string> response_data) {
  NetworkResponse response;
  if (url_loader->ResponseInfo() && url_loader->ResponseInfo()->headers) {
    response.status_code = url_loader->ResponseInfo()->headers->response_code();
    size_t iter = 0;
    std::string name;
    std::string value;
    while (url_loader->ResponseInfo()->headers->EnumerateHeaderLines(
        &iter, &name, &value)) {
      response.response_header_names_and_values.push_back(std::move(name));
      response.response_header_names_and_values.push_back(std::move(value));
    }
  } else {
    response.status_code = url_loader->NetError();
  }
  MetricsReporter::OnResourceFetched(response.status_code);

  if (response_data) {
    response.response_bytes = std::move(*response_data);
  }
  std::move(callback).Run(std::move(response));
}

}  // namespace feed
