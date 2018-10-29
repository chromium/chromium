// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/document_suggestions_service.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/omnibox/browser/document_provider.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/variations_associated_data.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/identity/public/cpp/primary_account_access_token_fetcher.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace {

// Builds a document search request body. Inputs are:
//   |query|: Current query text.
// The format of the request is:
//     {
//       query: "the search text",
//       start: 0,
//       pageSize: 10,
//       sourceOptions: [{source: {predefinedSource: "GOOGLE_DRIVE"}}]
//     }
std::string BuildDocumentSuggestionRequest(const base::string16& query) {
  base::Value root(base::Value::Type::DICTIONARY);
  root.SetKey("query", base::Value(query));
  root.SetKey("start", base::Value(0));
  root.SetKey("pageSize", base::Value(10));

  base::Value::ListStorage storage_options_list;
  base::Value source_definition(base::Value::Type::DICTIONARY);
  source_definition.SetPath({"source", "predefinedSource"},
                            base::Value("GOOGLE_DRIVE"));
  storage_options_list.emplace_back(std::move(source_definition));
  root.SetKey("dataSourceRestrictions",
              base::Value(std::move(storage_options_list)));

  std::string result;
  base::JSONWriter::Write(root, &result);
  return result;
}

}  // namespace

DocumentSuggestionsService::DocumentSuggestionsService(
    identity::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory),
      identity_manager_(identity_manager),
      token_fetcher_(nullptr) {
  DCHECK(url_loader_factory);
}

DocumentSuggestionsService::~DocumentSuggestionsService() {}

void DocumentSuggestionsService::CreateDocumentSuggestionsRequest(
    const base::string16& query,
    const TemplateURLService* template_url_service,
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
  // TODO(https://crbug.com/808498) re-add data use measurement once
  // SimpleURLLoader supports it.
  // We should attach data_use_measurement::DataUseUserData::OMNIBOX.

  // Create and fetch an OAuth2 token.
  std::string scope = "https://www.googleapis.com/auth/cloud_search.query";
  identity::ScopeSet scopes;
  scopes.insert(scope);
  token_fetcher_ = std::make_unique<identity::PrimaryAccountAccessTokenFetcher>(
      "document_suggestions_service", identity_manager_, scopes,
      base::BindOnce(&DocumentSuggestionsService::AccessTokenAvailable,
                     base::Unretained(this), std::move(request),
                     std::move(request_body), traffic_annotation,
                     std::move(start_callback), std::move(completion_callback)),
      identity::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable);
}

void DocumentSuggestionsService::StopCreatingDocumentSuggestionsRequest() {
  std::unique_ptr<identity::PrimaryAccountAccessTokenFetcher>
      token_fetcher_deleter(std::move(token_fetcher_));
}

void DocumentSuggestionsService::AccessTokenAvailable(
    std::unique_ptr<network::ResourceRequest> request,
    std::string request_body,
    net::NetworkTrafficAnnotationTag traffic_annotation,
    StartCallback start_callback,
    CompletionCallback completion_callback,
    GoogleServiceAuthError error,
    identity::AccessTokenInfo access_token_info) {
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
