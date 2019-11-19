// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/document_suggestions_service.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/values.h"
#include "components/omnibox/browser/document_provider.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/variations/net/variations_http_headers.h"
#include "components/variations/variations_associated_data.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace {

// Builds a document search request body. Inputs that affect the request are:
//   |query|: Current omnibox query text, passed as an argument.
//   |locale|: Current browser locale as BCP-47, obtained inside the function.
// The format of the request is:
//     {
//       query: "|query|",
//       start: 0,
//       pageSize: 10,
//       requestOptions: {
//            searchApplicationId: "searchapplications/chrome",
//            languageCode: "|locale|",
//       }
//     }
std::string BuildDocumentSuggestionRequest(const base::string16& query) {
  base::Value root(base::Value::Type::DICTIONARY);
  root.SetKey("query", base::Value(query));
  // The API supports pagination. We're always concerned with the first N
  // results on the first page.
  root.SetKey("start", base::Value(0));
  root.SetKey("pageSize", base::Value(10));

  base::Value request_options(base::Value::Type::DICTIONARY);
  request_options.SetKey("searchApplicationId",
                         base::Value("searchapplications/chrome"));
  request_options.SetKey("languageCode",
                         base::Value(base::i18n::GetConfiguredLocale()));
  root.SetKey("requestOptions", std::move(request_options));

  std::string result;
  base::JSONWriter::Write(root, &result);
  return result;
}

}  // namespace

DocumentSuggestionsService::DocumentSuggestionsService(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory),
      identity_manager_(identity_manager),
      token_fetcher_(nullptr) {
  DCHECK(url_loader_factory);
}

DocumentSuggestionsService::~DocumentSuggestionsService() {}

void DocumentSuggestionsService::CreateDocumentSuggestionsRequest(
    const base::string16& query,
    bool is_incognito,
    StartCallback start_callback,
    CompletionCallback completion_callback) {
  std::string endpoint = base::GetFieldTrialParamValueByFeature(
      omnibox::kDocumentProvider, "DocumentProviderEndpoint");
  if (endpoint.empty())
    endpoint = "https://cloudsearch.googleapis.com/v1/query/search";
  const GURL suggest_url = GURL(endpoint);
  DCHECK(suggest_url.is_valid());

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("omnibox_documentsuggest", R"(
        semantics {
          sender: "Omnibox"
          description:
            "Request for Google Drive document suggestions from the omnibox."
            "User must be signed in and have default search provider set to "
            "Google."
          trigger: "Signed-in user enters text in the omnibox."
          data: "The query string from the omnibox."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "Coupled to Google default search plus signed-in"
          chrome_policy {
            SearchSuggestEnabled {
                policy_options {mode: MANDATORY}
                SearchSuggestEnabled: false
            }
          }
        })");
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = suggest_url;
  request->method = "POST";
  std::string request_body = BuildDocumentSuggestionRequest(query);
  request->load_flags = net::LOAD_DO_NOT_SAVE_COOKIES;
  // It is expected that the user is signed in here. But we only care about
  // experiment IDs from the variations server, which do not require the
  // signed-in version of this method.
  variations::AppendVariationsHeaderUnknownSignedIn(
      request->url,
      is_incognito ? variations::InIncognito::kYes
                   : variations::InIncognito::kNo,
      request.get());

  // Create and fetch an OAuth2 token.
  std::string scope = "https://www.googleapis.com/auth/cloud_search.query";
  identity::ScopeSet scopes;
  scopes.insert(scope);
  token_fetcher_ = std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
      "document_suggestions_service", identity_manager_, scopes,
      base::BindOnce(&DocumentSuggestionsService::AccessTokenAvailable,
                     base::Unretained(this), std::move(request),
                     std::move(request_body), traffic_annotation,
                     std::move(start_callback), std::move(completion_callback)),
      signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable);
}

void DocumentSuggestionsService::StopCreatingDocumentSuggestionsRequest() {
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      token_fetcher_deleter(std::move(token_fetcher_));
}

void DocumentSuggestionsService::AccessTokenAvailable(
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

void DocumentSuggestionsService::StartDownloadAndTransferLoader(
    std::unique_ptr<network::ResourceRequest> request,
    std::string request_body,
    net::NetworkTrafficAnnotationTag traffic_annotation,
    StartCallback start_callback,
    CompletionCallback completion_callback) {
  std::unique_ptr<network::SimpleURLLoader> loader =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);
  if (!request_body.empty()) {
    loader->AttachStringForUpload(request_body, "application/json");
  }
  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(std::move(completion_callback), loader.get()));

  std::move(start_callback).Run(std::move(loader));
}
