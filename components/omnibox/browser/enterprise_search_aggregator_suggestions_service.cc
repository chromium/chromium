// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/enterprise_search_aggregator_suggestions_service.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "components/variations/net/variations_http_headers.h"
#include "net/base/load_flags.h"
#include "net/cookies/site_for_cookies.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

EnterpriseSearchAggregatorSuggestionsService::
    EnterpriseSearchAggregatorSuggestionsService(
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory) {
  DCHECK(url_loader_factory);
}

EnterpriseSearchAggregatorSuggestionsService::
    ~EnterpriseSearchAggregatorSuggestionsService() = default;

void EnterpriseSearchAggregatorSuggestionsService::
    CreateEnterpriseSearchAggregatorSuggestionsRequest(
        const GURL& suggest_url,
        const std::string& request_body,
        CreationCallback creation_callback,
        StartCallback start_callback,
        CompletionCallback completion_callback) {
  DCHECK(suggest_url.is_valid());

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = suggest_url;
  request->method = "POST";
  request->load_flags = net::LOAD_DO_NOT_SAVE_COOKIES;

  request->site_for_cookies = net::SiteForCookies::FromUrl(suggest_url);
  variations::AppendVariationsHeaderUnknownSignedIn(
      request->url, variations::InIncognito::kNo, request.get());

  std::move(creation_callback).Run(request.get());

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("omnibox_search_aggregator_suggest",
                                          R"(
        semantics {
          sender: "Omnibox"
          description:
            "Request for enterprise suggestions from the omnibox."
            "Enterprise suggestions provide enterprise specific documents"
            "to enterprise users and are configured by enterprise admin."
          trigger: "Signed-in enterprise user enters text in the omnibox."
          user_data {
            type: ACCESS_TOKEN
            type: USER_CONTENT
            type: SENSITIVE_URL
          }
          data: "The query string from the omnibox."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts { email: "chrome-desktop-search@google.com" }
          }
          last_reviewed: "2025-01-07"
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "The suggest_url is set by policy through enterprise admin."
            "Signed-in enterprise users can see this configuration in"
            "chrome://settings/searchEngines."
          chrome_policy {
            SearchSuggestEnabled {
                policy_options {mode: MANDATORY}
                SearchSuggestEnabled: false
            }
        }
      })");

  StartDownloadAndTransferLoader(std::move(request), std::move(request_body),
                                 traffic_annotation, std::move(start_callback),
                                 std::move(completion_callback));
}

// TODO(crbug.com/385756623): Factor out this method so it can be used across
//   document_suggestions_service and
//   enterprise_search_aggregator_suggestions_service.
void EnterpriseSearchAggregatorSuggestionsService::
    StartDownloadAndTransferLoader(
        std::unique_ptr<network::ResourceRequest> request,
        std::string request_body,
        net::NetworkTrafficAnnotationTag traffic_annotation,
        StartCallback start_callback,
        CompletionCallback completion_callback) {
  if (!url_loader_factory_) {
    return;
  }

  std::unique_ptr<network::SimpleURLLoader> loader =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);
  if (!request_body.empty()) {
    loader->AttachStringForUpload(request_body, "application/json");
  }
  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(std::move(completion_callback), loader.get()));

  std::move(start_callback).Run(std::move(loader), request_body);
}
