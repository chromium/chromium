// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/remote_suggestions_service.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/json/json_writer.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/omnibox/browser/base_search_provider.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/template_url_service.h"
#include "components/sync/base/time.h"
#include "components/variations/net/variations_http_headers.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace {

void AddVariationHeaders(network::ResourceRequest* request) {
  // Add Chrome experiment state to the request headers.
  //
  // Note: It's OK to pass InIncognito::kNo since we are expected to be in
  // non-incognito state here (i.e. remote sugestions are not served in
  // incognito mode).
  variations::AppendVariationsHeaderUnknownSignedIn(
      request->url, variations::InIncognito::kNo, request);
}

}  // namespace

RemoteSuggestionsService::RemoteSuggestionsService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory) {
  DCHECK(url_loader_factory);
}

RemoteSuggestionsService::~RemoteSuggestionsService() {}

// static
GURL RemoteSuggestionsService::EndpointUrl(
    TemplateURLRef::SearchTermsArgs search_terms_args,
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

  // Append a specific suggest client in ChromeOS app_list launcher contexts.
  BaseSearchProvider::AppendSuggestClientToAdditionalQueryParams(
      search_engine, search_terms_data, search_terms_args.page_classification,
      &search_terms_args);
  return GURL(suggestion_url_ref.ReplaceSearchTerms(search_terms_args,
                                                    search_terms_data));
}

void RemoteSuggestionsService::CreateSuggestionsRequest(
    const TemplateURLRef::SearchTermsArgs& search_terms_args,
    const TemplateURLService* template_url_service,
    StartCallback start_callback,
    CompletionCallback completion_callback) {
  const GURL suggest_url = EndpointUrl(search_terms_args, template_url_service);
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
  // Try to attach cookies for signed in user.
  request->attach_same_site_cookies = true;
  request->site_for_cookies = suggest_url;
  AddVariationHeaders(request.get());
  StartDownloadAndTransferLoader(std::move(request), std::string(),
                                 traffic_annotation, std::move(start_callback),
                                 std::move(completion_callback));
}

void RemoteSuggestionsService::StartDownloadAndTransferLoader(
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
