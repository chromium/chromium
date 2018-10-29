// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/contextual_suggestions_service.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/json/json_writer.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/omnibox/browser/base_search_provider.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/search_engines/template_url_service.h"
#include "components/sync/base/time.h"
#include "components/variations/net/variations_http_headers.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/identity/public/cpp/primary_account_access_token_fetcher.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace {

// Server address for the experimental suggestions service.
const char kDefaultExperimentalServerAddress[] =
    "https://cuscochromeextension-pa.googleapis.com/v1/omniboxsuggestions";

void AddVariationHeaders(network::ResourceRequest* request) {
  // Add Chrome experiment state to the request headers.
  //
  // Note: It's OK to pass InIncognito::kNo since we are expected to be in
  // non-incognito state here (i.e. contextual sugestions are not served in
  // incognito mode).
  variations::AppendVariationHeadersUnknownSignedIn(
      request->url, variations::InIncognito::kNo, &request->headers);
}

// Returns API request body. The final result depends on the following input
// variables:
//     * <current_url>: The current url visited by the user.
//     * <experiment_id>: the experiment id associated with the current field
//       trial group.
//
// The format of the request body is:
//
//     urls: {
//       url : <current_url>
//       // timestamp_usec is the timestamp for the page visit time, measured
//       // in microseconds since the Unix epoch.
//       timestamp_usec: <visit_time>
//     }
//     // stream_type = 1 corresponds to zero suggest suggestions.
//     stream_type: 1
//     // experiment_id is only set when <experiment_id> is well defined.
//     experiment_id: <experiment_id>
//
std::string FormatRequestBodyExperimentalService(const std::string& current_url,
                                                 const base::Time& visit_time) {
  auto request = std::make_unique<base::DictionaryValue>();
  auto url_list = std::make_unique<base::ListValue>();
  auto url_entry = std::make_unique<base::DictionaryValue>();
  url_entry->SetString("url", current_url);
  url_entry->SetString(
      "timestamp_usec",
      std::to_string((visit_time - base::Time::UnixEpoch()).InMicroseconds()));
  url_list->Append(std::move(url_entry));
  request->Set("urls", std::move(url_list));
  // stream_type = 1 corresponds to zero suggest suggestions.
  request->SetInteger("stream_type", 1);
  const int experiment_id =
      OmniboxFieldTrial::GetZeroSuggestRedirectToChromeExperimentId();
  if (experiment_id >= 0)
    request->SetInteger("experiment_id", experiment_id);
  std::string result;
  base::JSONWriter::Write(*request, &result);
  return result;
}

}  // namespace

ContextualSuggestionsService::ContextualSuggestionsService(
    identity::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory),
      identity_manager_(identity_manager),
      token_fetcher_(nullptr) {
  DCHECK(url_loader_factory);
}

ContextualSuggestionsService::~ContextualSuggestionsService() {}

void ContextualSuggestionsService::CreateContextualSuggestionsRequest(
    const std::string& current_url,
    const base::Time& visit_time,
    const AutocompleteInput& input,
    const TemplateURLService* template_url_service,
    StartCallback start_callback,
    CompletionCallback completion_callback) {
  const GURL experimental_suggest_url =
      ExperimentalContextualSuggestionsUrl(current_url, template_url_service);
  if (experimental_suggest_url.is_valid())
    CreateExperimentalRequest(current_url, visit_time, experimental_suggest_url,
                              std::move(start_callback),
                              std::move(completion_callback));
  else
    CreateDefaultRequest(current_url, input, template_url_service,
                         std::move(start_callback),
                         std::move(completion_callback));
}

void ContextualSuggestionsService::StopCreatingContextualSuggestionsRequest() {
  std::unique_ptr<identity::PrimaryAccountAccessTokenFetcher>
      token_fetcher_deleter(std::move(token_fetcher_));
}

// static
GURL ContextualSuggestionsService::ContextualSuggestionsUrl(
    const std::string& current_url,
    const AutocompleteInput& input,
    const TemplateURLService* template_url_service) {
  if (template_url_service == nullptr) {
    return GURL();
  }

  const TemplateURL* search_engine =
      template_url_service->GetDefaultSearchProvider();
  if (search_engine == nullptr) {
    return GURL();
  }

  const TemplateURLRef& suggestion_url_ref =
      search_engine->suggestions_url_ref();
  const SearchTermsData& search_terms_data =
      template_url_service->search_terms_data();
  base::string16 prefix;
  TemplateURLRef::SearchTermsArgs search_term_args(prefix);
  if (!current_url.empty()) {
    search_term_args.current_page_url = current_url;
  }

  search_term_args.page_classification = input.current_page_classification();

  // Append a specific suggest client in ChromeOS app_list launcher contexts.
  BaseSearchProvider::AppendSuggestClientToAdditionalQueryParams(
      search_engine, search_terms_data, input.current_page_classification(),
      &search_term_args);
  return GURL(suggestion_url_ref.ReplaceSearchTerms(search_term_args,
                                                    search_terms_data));
}

GURL ContextualSuggestionsService::ExperimentalContextualSuggestionsUrl(
    const std::string& current_url,
    const TemplateURLService* template_url_service) const {
  if (current_url.empty() || template_url_service == nullptr) {
    return GURL();
  }

  if (!base::FeatureList::IsEnabled(omnibox::kZeroSuggestRedirectToChrome)) {
    return GURL();
  }

  // Check that the default search engine is Google.
  const TemplateURL& default_provider_url =
      *template_url_service->GetDefaultSearchProvider();
  const SearchTermsData& search_terms_data =
      template_url_service->search_terms_data();
  if (default_provider_url.GetEngineType(search_terms_data) !=
      SEARCH_ENGINE_GOOGLE) {
    return GURL();
  }

  const std::string server_address_param =
      OmniboxFieldTrial::GetZeroSuggestRedirectToChromeServerAddress();
  GURL suggest_url(server_address_param.empty()
                       ? kDefaultExperimentalServerAddress
                       : server_address_param);
  // Check that the suggest URL for redirect to chrome field trial is valid.
  if (!suggest_url.is_valid()) {
    return GURL();
  }

  // Check that the suggest URL for redirect to chrome is HTTPS.
  if (!suggest_url.SchemeIsCryptographic()) {
    return GURL();
  }

  return suggest_url;
}

void ContextualSuggestionsService::CreateDefaultRequest(
    const std::string& current_url,
    const AutocompleteInput& input,
    const TemplateURLService* template_url_service,
    StartCallback start_callback,
    CompletionCallback completion_callback) {
  const GURL suggest_url =
      ContextualSuggestionsUrl(current_url, input, template_url_service);
  DCHECK(suggest_url.is_valid());

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("omnibox_zerosuggest", R"(
        semantics {
          sender: "Omnibox"
          description:
            "When the user focuses the omnibox, Chrome can provide search or "
            "navigation suggestions from the default search provider in the "
            "omnibox dropdown, based on the current page URL.\n"
            "This is limited to users whose default search engine is Google, "
            "as no other search engines currently support this kind of "
            "suggestion."
          trigger: "The omnibox receives focus."
          data: "The URL of the current page."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "Users can control this feature via the 'Use a prediction service "
            "to help complete searches and URLs typed in the address bar' "
            "settings under 'Privacy'. The feature is enabled by default."
          chrome_policy {
            SearchSuggestEnabled {
                policy_options {mode: MANDATORY}
                SearchSuggestEnabled: false
            }
          }
        })");

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = suggest_url;
  request->load_flags = net::LOAD_DO_NOT_SAVE_COOKIES;
  AddVariationHeaders(request.get());
  // TODO(https://crbug.com/808498) re-add data use measurement once
  // SimpleURLLoader supports it.
  // data_use_measurement::DataUseUserData::OMNIBOX
  StartDownloadAndTransferLoader(std::move(request), std::string(),
                                 traffic_annotation, std::move(start_callback),
                                 std::move(completion_callback));
}

void ContextualSuggestionsService::CreateExperimentalRequest(
    const std::string& current_url,
    const base::Time& visit_time,
    const GURL& suggest_url,
    StartCallback start_callback,
    CompletionCallback completion_callback) {
  DCHECK(suggest_url.is_valid());

  // This traffic annotation is nearly identical to the annotation for
  // `omnibox_zerosuggest`. The main difference is that the experimental traffic
  // is not allowed cookies.
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("omnibox_zerosuggest_experimental",
                                          R"(
        semantics {
          sender: "Omnibox"
          description:
            "When the user focuses the omnibox, Chrome can provide search or "
            "navigation suggestions from the default search provider in the "
            "omnibox dropdown, based on the current page URL.\n"
            "This is limited to users whose default search engine is Google, "
            "as no other search engines currently support this kind of "
            "suggestion."
          trigger: "The omnibox receives focus."
          data: "The user's OAuth2 credentials and the URL of the current page."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can control this feature via the 'Use a prediction service "
            "to help complete searches and URLs typed in the address bar' "
            "settings under 'Privacy'. The feature is enabled by default."
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
  std::string request_body =
      FormatRequestBodyExperimentalService(current_url, visit_time);
  AddVariationHeaders(request.get());
  request->load_flags =
      net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES;
  // TODO(https://crbug.com/808498) re-add data use measurement once
  // SimpleURLLoader supports it.
  // data_use_measurement::DataUseUserData::OMNIBOX

  // If authentication services are unavailable or if this request is still
  // waiting for an oauth2 token, run the contextual service without access
  // tokens.
  if ((identity_manager_ == nullptr) || (token_fetcher_ != nullptr)) {
    StartDownloadAndTransferLoader(
        std::move(request), std::move(request_body), traffic_annotation,
        std::move(start_callback), std::move(completion_callback));
    return;
  }

  // Create the oauth2 token fetcher.
  const identity::ScopeSet scopes{
      "https://www.googleapis.com/auth/cusco-chrome-extension"};
  token_fetcher_ = std::make_unique<identity::PrimaryAccountAccessTokenFetcher>(
      "contextual_suggestions_service", identity_manager_, scopes,
      base::BindOnce(&ContextualSuggestionsService::AccessTokenAvailable,
                     base::Unretained(this), std::move(request),
                     std::move(request_body), traffic_annotation,
                     std::move(start_callback), std::move(completion_callback)),
      identity::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable);
}

void ContextualSuggestionsService::AccessTokenAvailable(
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

void ContextualSuggestionsService::StartDownloadAndTransferLoader(
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
