// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search/start_suggest_service.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "components/omnibox/browser/autocomplete_scheme_classifier.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/search/search_provider_observer.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace {
// A cache of trending query suggestions using JSON serialized into a string.
const char kTrendingQuerySuggestionCachedResults[] =
    "TrendingQuerySuggestionCachedResults";

const char kXSSIResponsePreamble[] = ")]}'";
}  // namespace

StartSuggestService::StartSuggestService(
    TemplateURLService* template_url_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<AutocompleteSchemeClassifier> scheme_classifier,
    const std::string& application_country,
    const std::string& application_locale,
    const GURL& request_initiator_url)
    : template_url_service_(template_url_service),
      url_loader_factory_(url_loader_factory),
      scheme_classifier_(std::move(scheme_classifier)),
      application_country_(application_country),
      application_locale_(application_locale),
      request_initiator_url_(request_initiator_url),
      search_provider_observer_(std::make_unique<SearchProviderObserver>(
          template_url_service_,
          base::BindRepeating(&StartSuggestService::SearchProviderChanged,
                              base::Unretained(this)))) {
  DCHECK(template_url_service);
  DCHECK(url_loader_factory_);
  DCHECK(scheme_classifier_);
}

StartSuggestService::~StartSuggestService() = default;

void StartSuggestService::FetchSuggestions(
    const TemplateURLRef::SearchTermsArgs& args,
    SuggestResultCallback callback,
    bool fetch_from_server) {
  // Do nothing if Google not default search engine.
  if (!search_provider_observer()->is_google()) {
    std::move(callback).Run(QuerySuggestions());
    return;
  }

  // If there are saved suggestions from a previous request, return that.
  if (!fetch_from_server &&
      suggestions_cache_.find(kTrendingQuerySuggestionCachedResults) !=
          suggestions_cache_.end()) {
    QuerySuggestions cache =
        suggestions_cache_[kTrendingQuerySuggestionCachedResults];
    if (!cache.empty()) {
      std::move(callback).Run(std::move(cache));
      return;
    }
  }

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("chrome_search_suggest_service",
                                          R"(
        semantics {
          sender: "Chrome Search Suggest Service"
          description:
            "Fetch query suggestions to be shown in NTP."
          trigger:
            "Displaying on the new tab page, if Google is the "
            "configured search provider."
          data: "None"
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can control this feature by selecting a non-Google default "
            "search engine in Chrome settings under 'Search Engine'"
          chrome_policy {
            DefaultSearchProviderEnabled {
              policy_options {mode: MANDATORY}
              DefaultSearchProviderEnabled: false
            }
          }
        })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  const GURL& request_url = GetRequestURL(args);

  resource_request->url = request_url;
  // Do not send credentials since Trending Queries is locale-based.
  resource_request->credentials_mode =
      network::mojom::CredentialsMode::kOmitBug_775438_Workaround;
  resource_request->request_initiator =
      url::Origin::Create(request_initiator_url_);

  loaders_.push_back(network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation));
  loaders_.back()->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&StartSuggestService::SuggestResponseLoaded,
                     weak_ptr_factory_.GetWeakPtr(), loaders_.back().get(),
                     std::move(callback)),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

void StartSuggestService::SearchProviderChanged() {
  // Remove cached results if the default search engine changes.
  suggestions_cache_.clear();
}

GURL StartSuggestService::GetRequestURL(
    const TemplateURLRef::SearchTermsArgs& search_terms_args) {
  const TemplateURL* default_provider =
      template_url_service_->GetDefaultSearchProvider();
  DCHECK(default_provider);
  const TemplateURLRef& suggestion_url_ref =
      default_provider->suggestions_url_ref();
  const SearchTermsData& search_terms_data =
      template_url_service_->search_terms_data();
  DCHECK(suggestion_url_ref.SupportsReplacement(search_terms_data));
  GURL url = GURL(suggestion_url_ref.ReplaceSearchTerms(search_terms_args,
                                                        search_terms_data));
  if (!application_country_.empty()) {
    // Trending Queries are country-based, so passing this helps determine
    // locale.
    url = net::AppendQueryParameter(url, "gl", application_country_);
  }
  if (!application_locale_.empty()) {
    // Language is also used in addition to country to rank suggestions,
    // ensuring that there can be separate ranks for different languages in the
    // same country (i.e. fr-ca and en-ca.
    url = net::AppendQueryParameter(url, "hl", application_locale_);
  }
  return url;
}

GURL StartSuggestService::GetQueryDestinationURL(
    const std::u16string& query,
    const TemplateURL* search_provider) {
  TemplateURLRef::SearchTermsArgs search_terms_args(query);
  DCHECK(search_provider);
  const TemplateURLRef& search_url_ref = search_provider->url_ref();
  const SearchTermsData& search_terms_data =
      template_url_service_->search_terms_data();
  DCHECK(search_url_ref.SupportsReplacement(search_terms_data));
  return GURL(
      search_url_ref.ReplaceSearchTerms(search_terms_args, search_terms_data));
}

SearchProviderObserver* StartSuggestService::search_provider_observer() {
  return search_provider_observer_.get();
}

void StartSuggestService::SuggestResponseLoaded(
    network::SimpleURLLoader* loader,
    SuggestResultCallback callback,
    std::unique_ptr<std::string> response) {
  // Ensure the request succeeded and that the provider used is still available.
  // A verbatim match cannot be generated without this provider, causing errors.
  const bool request_succeeded = response && loader->NetError() == net::OK;
  std::erase_if(loaders_, [loader](const auto& loader_ptr) {
    return loader == loader_ptr.get();
  });
  if (!request_succeeded) {
    std::move(callback).Run(QuerySuggestions());
    return;
  }

  if (base::StartsWith(*response, kXSSIResponsePreamble,
                       base::CompareCase::SENSITIVE)) {
    *response = response->substr(strlen(kXSSIResponsePreamble));
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      *response,
      base::BindOnce(&StartSuggestService::SuggestionsParsed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void StartSuggestService::SuggestionsParsed(
    SuggestResultCallback callback,
    data_decoder::DataDecoder::ValueOrError result) {
  std::move(callback).Run([&] {
    QuerySuggestions query_suggestions;
    if (result.has_value() && result.value().is_list()) {
      SearchSuggestionParser::Results results;
      AutocompleteInput input;
      if (SearchSuggestionParser::ParseSuggestResults(
              result->GetList(), input, *scheme_classifier_,
              /*default_result_relevance=*/-1, /*is_keyword_result=*/false,
              &results)) {
        for (SearchSuggestionParser::SuggestResult suggest :
             results.suggest_results) {
          QuerySuggestion query;
          query.query = suggest.suggestion();
          query.destination_url = GetQueryDestinationURL(
              query.query, template_url_service_->GetDefaultSearchProvider());
          query_suggestions.push_back(std::move(query));
        }
        suggestions_cache_[kTrendingQuerySuggestionCachedResults] =
            query_suggestions;
      }
    }
    return query_suggestions;
  }());
}
