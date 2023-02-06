// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/remote_suggestions_service.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "components/omnibox/browser/base_search_provider.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/net/variations_http_headers.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

void AddVariationHeaders(network::ResourceRequest* request) {
  // Note: It's OK to pass InIncognito::kNo since we are expected to be in
  // non-incognito state here (i.e. remote suggestions are not served in
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

RemoteSuggestionsService::~RemoteSuggestionsService() = default;

// static
GURL RemoteSuggestionsService::EndpointUrl(
    const TemplateURL* template_url,
    TemplateURLRef::SearchTermsArgs search_terms_args,
    const SearchTermsData& search_terms_data) {
  const TemplateURLRef& suggestion_url_ref =
      template_url->suggestions_url_ref();

  // Append a specific suggest client in ChromeOS app_list launcher contexts.
  BaseSearchProvider::AppendSuggestClientToAdditionalQueryParams(
      template_url, search_terms_data, search_terms_args.page_classification,
      &search_terms_args);
  return GURL(suggestion_url_ref.ReplaceSearchTerms(search_terms_args,
                                                    search_terms_data));
}

std::unique_ptr<network::SimpleURLLoader>
RemoteSuggestionsService::StartSuggestionsRequest(
    const TemplateURL* template_url,
    TemplateURLRef::SearchTermsArgs search_terms_args,
    const SearchTermsData& search_terms_data,
    CompletionCallback completion_callback) {
  DCHECK(template_url);

  const GURL suggest_url =
      EndpointUrl(template_url, search_terms_args, search_terms_data);
  if (!suggest_url.is_valid()) {
    return nullptr;
  }

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("omnibox_suggest", R"(
        semantics {
          sender: "Omnibox"
          description:
            "Chrome can provide search and navigation suggestions from the "
            "currently-selected search provider in the omnibox dropdown, based "
            "on user input."
          trigger: "User typing in the omnibox."
          data:
            "The text typed into the address bar. Potentially other metadata, "
            "such as the current cursor position or URL of the current page."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "Users can control this feature via the 'Use a prediction service "
            "to help complete searches and URLs typed in the address bar' "
            "setting under 'Privacy'. The feature is enabled by default."
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
  // Add Chrome experiment state to the request headers.
  AddVariationHeaders(request.get());

  // Create a unique identifier for the request.
  const base::UnguessableToken request_id = base::UnguessableToken::Create();

  // Notify the observers that the transfer is about to start.
  for (Observer& observer : observers_) {
    observer.OnSuggestRequestStarting(request_id, request.get());
  }

  // Make loader and start download.
  std::unique_ptr<network::SimpleURLLoader> loader =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);
  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&RemoteSuggestionsService::OnURLLoadComplete,
                     weak_ptr_factory_.GetWeakPtr(), request_id,
                     std::move(completion_callback), loader.get()));
  return loader;
}

std::unique_ptr<network::SimpleURLLoader>
RemoteSuggestionsService::StartZeroPrefixSuggestionsRequest(
    const TemplateURL* template_url,
    TemplateURLRef::SearchTermsArgs search_terms_args,
    const SearchTermsData& search_terms_data,
    CompletionCallback completion_callback) {
  DCHECK(template_url);

  const GURL suggest_url =
      EndpointUrl(template_url, search_terms_args, search_terms_data);
  if (!suggest_url.is_valid()) {
    return nullptr;
  }

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
  if (search_terms_args.bypass_cache) {
    request->load_flags |= net::LOAD_BYPASS_CACHE;
  }
  // Try to attach cookies for signed in user.
  request->site_for_cookies = net::SiteForCookies::FromUrl(suggest_url);
  // Add Chrome experiment state to the request headers.
  AddVariationHeaders(request.get());

  // Create a unique identifier for the request.
  const base::UnguessableToken request_id = base::UnguessableToken::Create();

  // Notify the observers that the transfer is about to start.
  for (Observer& observer : observers_) {
    observer.OnSuggestRequestStarting(request_id, request.get());
  }

  // Make loader and start download.
  std::unique_ptr<network::SimpleURLLoader> loader =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);
  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&RemoteSuggestionsService::OnURLLoadComplete,
                     weak_ptr_factory_.GetWeakPtr(), request_id,
                     std::move(completion_callback), loader.get()));
  return loader;
}

std::unique_ptr<network::SimpleURLLoader>
RemoteSuggestionsService::StartDeletionRequest(
    const std::string& deletion_url,
    CompletionCallback completion_callback) {
  const GURL url(deletion_url);
  DCHECK(url.is_valid());

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("omnibox_suggest_deletion", R"(
        semantics {
          sender: "Omnibox"
          description:
            "When users attempt to delete server-provided personalized search "
            "or navigation suggestions from the omnibox dropdown, Chrome sends "
            "a message to the server requesting deletion of the suggestion."
          trigger:
            "A user attempt to delete a server-provided omnibox suggestion, "
            "for which the server provided a custom deletion URL."
          data:
            "No user data is explicitly sent with the request, but because the "
            "requested URL is provided by the server for each specific "
            "suggestion, it necessarily uniquely identifies the suggestion the "
            "user is attempting to delete."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "Since this can only be triggered on seeing server-provided "
            "suggestions in the omnibox dropdown, whether it is enabled is the "
            "same as whether those suggestions are enabled.\n"
            "Users can control this feature via the 'Use a prediction service "
            "to help complete searches and URLs typed in the address bar' "
            "setting under 'Privacy'. The feature is enabled by default."
          chrome_policy {
            SearchSuggestEnabled {
                policy_options {mode: MANDATORY}
                SearchSuggestEnabled: false
            }
          }
        })");
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url;
  // Add Chrome experiment state to the request headers.
  AddVariationHeaders(request.get());

  // Create a unique identifier for the request.
  const base::UnguessableToken request_id = base::UnguessableToken::Create();

  // Notify the observers that the transfer is about to start.
  for (Observer& observer : observers_) {
    observer.OnSuggestRequestStarting(request_id, request.get());
  }

  // Make loader and start download.
  std::unique_ptr<network::SimpleURLLoader> loader =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);
  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&RemoteSuggestionsService::OnURLLoadComplete,
                     weak_ptr_factory_.GetWeakPtr(), request_id,
                     std::move(completion_callback), loader.get()));
  return loader;
}

void RemoteSuggestionsService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void RemoteSuggestionsService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void RemoteSuggestionsService::OnURLLoadComplete(
    const base::UnguessableToken& request_id,
    CompletionCallback completion_callback,
    const network::SimpleURLLoader* source,
    std::unique_ptr<std::string> response_body) {
  const bool response_received =
      response_body && source->NetError() == net::OK &&
      source->ResponseInfo() && source->ResponseInfo()->headers &&
      source->ResponseInfo()->headers->response_code() == 200;

  // Notify the observers that the transfer is done.
  for (Observer& observer : observers_) {
    observer.OnSuggestRequestCompleted(request_id, response_received,
                                       response_body);
  }

  std::move(completion_callback)
      .Run(source, response_received, std::move(response_body));
}
