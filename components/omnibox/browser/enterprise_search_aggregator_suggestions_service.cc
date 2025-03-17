// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/enterprise_search_aggregator_suggestions_service.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "components/variations/net/variations_http_headers.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/base/load_flags.h"
#include "net/cookies/site_for_cookies.h"
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
// The format of the request is:
//     {
//       query: "`query`",
//       suggestionTypes: `suggestion_types`
//     }
std::string BuildRequestBody(std::u16string query,
                             std::vector<int>& suggestion_types) {
  base::Value::Dict root;
  root.Set("query", query);

  base::Value::List suggestion_types_list;
  for (const auto& item : suggestion_types) {
    suggestion_types_list.Append(item);
  }

  root.Set("suggestionTypes", std::move(suggestion_types_list));
  std::string result;
  base::JSONWriter::Write(root, &result);
  return result;
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
        CreationCallback creation_callback,
        StartCallback start_callback,
        CompletionCallback completion_callback,
        bool in_keyword_mode) {
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

  // For now, exclude recent suggestions (4) and, outside of keyword mode,
  // search suggestions (1).
  // TODO(crbug.com/393480150): Support recent suggestions.
  auto suggestion_types_list = in_keyword_mode ? std::vector<int>{1, 2, 3, 5}
                                               : std::vector<int>{2, 3, 5};

  const std::string& request_body =
      BuildRequestBody(query, suggestion_types_list);

  // Create and fetch an OAuth2 token.
  signin::ScopeSet scopes;

  if (omnibox_feature_configs::SearchAggregatorProvider::Get()
          .use_discovery_engine_oauth_scope) {
    scopes.insert(GaiaConstants::kDiscoveryEngineCompleteQueryOAuth2Scope);
  } else {
    scopes.insert(GaiaConstants::kCloudSearchQueryOAuth2Scope);
  }
  token_fetcher_ = std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
      "enterprise_search_aggregator_suggestions_service", identity_manager_,
      scopes,
      base::BindOnce(
          &EnterpriseSearchAggregatorSuggestionsService::AccessTokenAvailable,
          base::Unretained(this), std::move(request), std::move(request_body),
          traffic_annotation, std::move(start_callback),
          std::move(completion_callback)),
      signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable,
      signin::ConsentLevel::kSignin);
}

void EnterpriseSearchAggregatorSuggestionsService::AccessTokenAvailable(
    std::unique_ptr<network::ResourceRequest> request,
    std::string request_body,
    net::NetworkTrafficAnnotationTag traffic_annotation,
    StartCallback start_callback,
    CompletionCallback completion_callback,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  DCHECK(token_fetcher_);
  token_fetcher_.reset();
  // If there were no errors obtaining the access token, append it to the
  // request as a header.
  if (error.state() == GoogleServiceAuthError::NONE) {
    DCHECK(!access_token_info.token.empty());
    request->headers.SetHeader(
        "Authorization",
        base::StringPrintf("Bearer %s", access_token_info.token.c_str()));
  }

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
