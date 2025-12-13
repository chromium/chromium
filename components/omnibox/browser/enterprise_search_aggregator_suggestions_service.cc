// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/enterprise_search_aggregator_suggestions_service.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "components/variations/net/variations_http_headers.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/base/load_flags.h"
#include "net/cookies/site_for_cookies.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace {
// Builds an enterprise search aggregator request body. Inputs that affect the
// request are:
//   `query`: Current omnibox query text, passed as an argument.
//   `suggestion_types`: Types of suggestions requested. Ex. [1,2] indicates
//   Query and Person Suggestions respectively.
//   `experiment_id`: Hardcoded experiment ID expected by the server.
// The format of the request is:
//     {
//       query: "`query`",
//       suggestionTypes: `suggestion_types`,
//       experimentIds: ["`experiment_id`"]
//     }
std::string BuildRequestBody(std::u16string query,
                             const std::vector<int>& suggestion_types) {
  base::Value::Dict root;
  root.Set("query", query);

  base::Value::List suggestion_types_list;
  for (const auto& item : suggestion_types) {
    suggestion_types_list.Append(item);
  }
  root.Set("suggestionTypes", std::move(suggestion_types_list));

  base::Value::List experiment_ids_list;
  experiment_ids_list.Append(kEnterpriseSearchAggregatorExperimentId);
  root.Set("experimentIds", std::move(experiment_ids_list));

  return base::WriteJson(root).value_or("");
}
}  // namespace

EnterpriseSearchAggregatorSuggestionsService::
    EnterpriseSearchAggregatorSuggestionsService(
        signin::IdentityManager* identity_manager,
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory),
      identity_manager_(identity_manager),
      token_fetcher_(nullptr) {}

EnterpriseSearchAggregatorSuggestionsService::
    ~EnterpriseSearchAggregatorSuggestionsService() = default;

void EnterpriseSearchAggregatorSuggestionsService::
    CreateEnterpriseSearchAggregatorSuggestionsRequest(
        const std::u16string& query,
        const GURL& suggest_url,
        std::vector<int> callback_indexes,
        std::vector<std::vector<int>> suggestion_types,
        CreationCallback creation_callback,
        StartCallback start_callback,
        CompletionCallback completion_callback) {
  DCHECK(suggest_url.is_valid());
  CHECK_EQ(callback_indexes.size(), suggestion_types.size());

  auto requests = std::vector<std::unique_ptr<network::ResourceRequest>>{};
  for (size_t i = 0; i < suggestion_types.size(); ++i) {
    requests.push_back(std::make_unique<network::ResourceRequest>());
  }
  for (size_t i = 0; i < suggestion_types.size(); i++) {
    requests[i]->url = suggest_url;
    requests[i]->method = net::HttpRequestHeaders::kPostMethod;
    requests[i]->load_flags = net::LOAD_DO_NOT_SAVE_COOKIES;

    requests[i]->site_for_cookies = net::SiteForCookies::FromUrl(suggest_url);
    variations::AppendVariationsHeaderUnknownSignedIn(
        requests[i]->url, variations::InIncognito::kNo, requests[i].get());

    creation_callback.Run(requests[i].get());
  }

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("omnibox_search_aggregator_suggest",
                                          R"(
        semantics {
          sender: "Omnibox"
          description:
            "Request for enterprise suggestions from the omnibox. Enterprise "
            "suggestions provide enterprise specific documents to enterprise "
            "users and are configured by enterprise admin."
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

  token_fetcher_ = std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
      signin::OAuthConsumerId::kEnterpriseSearchAggregator, identity_manager_,
      base::BindOnce(
          &EnterpriseSearchAggregatorSuggestionsService::AccessTokenAvailable,
          base::Unretained(this), std::move(requests), std::move(query),
          std::move(callback_indexes), std::move(suggestion_types),
          traffic_annotation, std::move(start_callback),
          std::move(completion_callback)),
      signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable,
      signin::ConsentLevel::kSignin);
}

void EnterpriseSearchAggregatorSuggestionsService::
    StopCreatingEnterpriseSearchAggregatorSuggestionsRequest() {
  token_fetcher_.reset();
}

void EnterpriseSearchAggregatorSuggestionsService::AccessTokenAvailable(
    std::vector<std::unique_ptr<network::ResourceRequest>> requests,
    const std::u16string& query,
    std::vector<int> callback_indexes,
    std::vector<std::vector<int>> suggestion_types,
    net::NetworkTrafficAnnotationTag traffic_annotation,
    StartCallback start_callback,
    CompletionCallback completion_callback,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  DCHECK(token_fetcher_);
  token_fetcher_.reset();

  auto request_bodies = std::vector<std::string>{};
  for (const auto& suggestion_type : suggestion_types) {
    const std::string& request_body = BuildRequestBody(query, suggestion_type);
    request_bodies.push_back(request_body);
  }

  // If there were no errors obtaining the access token, append it to the
  // request as a header.
  if (error.state() == GoogleServiceAuthError::NONE) {
    DCHECK(!access_token_info.token.empty());
    for (const auto& request : requests) {
      request->headers.SetHeader(
          "Authorization",
          base::StringPrintf("Bearer %s", access_token_info.token.c_str()));
    }
  }

  for (size_t i = 0; i < requests.size(); ++i) {
    StartDownloadAndTransferLoader(std::move(requests[i]),
                                   std::move(request_bodies[i]),
                                   traffic_annotation, callback_indexes[i],
                                   start_callback, completion_callback);
  }
}

// TODO(crbug.com/385756623): Factor out this method so it can be used across
//   document_suggestions_service and
//   enterprise_search_aggregator_suggestions_service.
void EnterpriseSearchAggregatorSuggestionsService::
    StartDownloadAndTransferLoader(
        std::unique_ptr<network::ResourceRequest> request,
        std::string request_body,
        net::NetworkTrafficAnnotationTag traffic_annotation,
        int request_index,
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
      base::BindOnce(completion_callback, loader.get(), request_index));

  std::move(start_callback).Run(request_index, std::move(loader), request_body);
}
