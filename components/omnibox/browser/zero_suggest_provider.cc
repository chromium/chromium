// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/zero_suggest_provider.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/json/json_string_value_serializer.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/top_sites.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/remote_suggestions_service.h"
#include "components/omnibox/browser/search_provider.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/omnibox/browser/verbatim_match.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/omnibox_focus_type.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/template_url_service.h"
#include "components/url_formatter/url_formatter.h"
#include "components/variations/net/variations_http_headers.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "url/gurl.h"

using metrics::OmniboxEventProto;

namespace {

// Represents whether ZeroSuggestProvider is allowed to display contextual
// suggestions on focus, and if not, why not.
// These values are written to logs.  New enum values can be added, but existing
// enums must never be renumbered or deleted and reused.
enum class ZeroSuggestEligibility {
  ELIGIBLE = 0,
  // URL_INELIGIBLE would be ELIGIBLE except some property of the current URL
  // itself prevents ZeroSuggest from triggering.
  URL_INELIGIBLE = 1,
  GENERALLY_INELIGIBLE = 2,
  ELIGIBLE_MAX_VALUE
};

// Keeps track of how many Suggest requests are sent, how many requests were
// invalidated, e.g., due to user starting to type, how many responses were
// received, how many of those responses were loaded from the HTTP cache, and of
// those cached responses, how many were out-of-date.
// These values are written to logs.  New enum values can be added, but existing
// enums must never be renumbered or deleted and reused.
enum ZeroSuggestRequestsHistogramValue {
  ZERO_SUGGEST_REQUEST_SENT = 1,
  ZERO_SUGGEST_REQUEST_INVALIDATED = 2,
  ZERO_SUGGEST_RESPONSE_RECEIVED = 3,
  ZERO_SUGGEST_RESPONSE_LOADED_FROM_HTTP_CACHE = 4,
  ZERO_SUGGEST_CACHED_RESPONSE_IS_OUT_OF_DATE = 5,
  ZERO_SUGGEST_MAX_REQUEST_HISTOGRAM_VALUE
};

void LogOmniboxZeroSuggestRequest(
    ZeroSuggestRequestsHistogramValue request_value,
    bool is_prefetch) {
  base::UmaHistogramEnumeration(
      is_prefetch ? "Omnibox.ZeroSuggestRequests.Prefetch"
                  : "Omnibox.ZeroSuggestRequests.NonPrefetch",
      request_value, ZERO_SUGGEST_MAX_REQUEST_HISTOGRAM_VALUE);
}

void LogOmniboxZeroSuggestRequestRoundTripTime(base::TimeDelta round_trip_time,
                                               bool is_prefetch) {
  base::UmaHistogramTimes(
      is_prefetch ? "Omnibox.ZeroSuggestRequests.Prefetch.RoundTripTime"
                  : "Omnibox.ZeroSuggestRequests.NonPrefetch.RoundTripTime",
      round_trip_time);
}

// Relevance value to use if it was not set explicitly by the server.
const int kDefaultZeroSuggestRelevance = 100;

// Used for testing whether zero suggest is ever available.
constexpr char kArbitraryInsecureUrlString[] = "http://www.google.com/";

// Metric name tracking the omnibox suggestion eligibility.
constexpr char kOmniboxZeroSuggestEligibleHistogramName[] =
    "Omnibox.ZeroSuggest.Eligible.OnFocusV2";

// Remote suggestions are allowed only if the user is signed-in and has Google
// set up as their default search engine. The authentication state check is done
// not for privacy reasons but to prevent signed-out users from querying the
// server which does not have any suggestions for them. This check is skipped if
// |check_authentication_state| is false.
// This function only applies to kRemoteNoUrlVariant. For kRemoteSendUrlVariant,
// most of these checks with the exception of the authentication state are done
// in BaseSearchProvider::CanSendURL().
bool RemoteNoUrlSuggestionsAreAllowed(
    AutocompleteProviderClient* client,
    const TemplateURLService* template_url_service,
    bool check_authentication_state) {
  if (!client->SearchSuggestEnabled())
    return false;

  if (check_authentication_state && !client->IsAuthenticated())
    return false;

  if (template_url_service == nullptr)
    return false;

  const TemplateURL* default_provider =
      template_url_service->GetDefaultSearchProvider();
  return default_provider &&
         default_provider->GetEngineType(
             template_url_service->search_terms_data()) == SEARCH_ENGINE_GOOGLE;
}

}  // namespace

// static
ZeroSuggestProvider* ZeroSuggestProvider::Create(
    AutocompleteProviderClient* client,
    AutocompleteProviderListener* listener) {
  return new ZeroSuggestProvider(client, listener);
}

// static
void ZeroSuggestProvider::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(omnibox::kZeroSuggestCachedResults,
                               std::string());
}

void ZeroSuggestProvider::Start(const AutocompleteInput& input,
                                bool minimal_changes) {
  Start(input, minimal_changes, /*is_prefetch=*/false, /*bypass_cache=*/false);
}

void ZeroSuggestProvider::StartPrefetch(const AutocompleteInput& input) {
  Start(input, /*minimal_changes=*/false, /*is_prefetch=*/true,
        /*bypass_cache=*/
        OmniboxFieldTrial::kZeroSuggestPrefetchBypassCache.Get());
}

void ZeroSuggestProvider::Start(const AutocompleteInput& input,
                                bool minimal_changes,
                                bool is_prefetch,
                                bool bypass_cache) {
  TRACE_EVENT0("omnibox", "ZeroSuggestProvider::Start");
  matches_.clear();
  Stop(true, false);

  if (!AllowZeroSuggestSuggestions(input)) {
    UMA_HISTOGRAM_ENUMERATION(kOmniboxZeroSuggestEligibleHistogramName,
                              ZeroSuggestEligibility::GENERALLY_INELIGIBLE,
                              ZeroSuggestEligibility::ELIGIBLE_MAX_VALUE);
    return;
  }

  result_type_running_ = NONE;
  set_field_trial_triggered(false);
  set_field_trial_triggered_in_session(false);
  permanent_text_ = input.text();
  current_query_ = input.current_url().spec();
  current_title_ = input.current_title();
  current_page_classification_ = input.current_page_classification();
  current_text_match_ = MatchForCurrentText();

  TemplateURLRef::SearchTermsArgs search_terms_args;
  search_terms_args.page_classification = current_page_classification_;
  search_terms_args.focus_type = input.focus_type();
  const int cache_duration_sec =
      OmniboxFieldTrial::kZeroSuggestCacheDurationSec.Get();
  if (cache_duration_sec > 0) {
    search_terms_args.zero_suggest_cache_duration_sec = cache_duration_sec;
  }
  search_terms_args.bypass_cache = bypass_cache;
  GURL suggest_url = RemoteSuggestionsService::EndpointUrl(
      search_terms_args, client()->GetTemplateURLService());
  if (!suggest_url.is_valid())
    return;

  result_type_running_ = TypeOfResultToRun(client(), input, suggest_url);
  if (result_type_running_ == NONE)
    return;

  done_ = false;

  const std::string original_response = MaybeUseStoredResponse();

  search_terms_args.current_page_url =
      result_type_running_ == REMOTE_SEND_URL ? current_query_ : std::string();
  client()
      ->GetRemoteSuggestionsService(/*create_if_necessary=*/true)
      ->CreateSuggestionsRequest(
          search_terms_args, client()->GetTemplateURLService(),
          base::BindOnce(
              &ZeroSuggestProvider::OnRemoteSuggestionsLoaderAvailable,
              weak_ptr_factory_.GetWeakPtr(), is_prefetch),
          base::BindOnce(&ZeroSuggestProvider::OnURLLoadComplete,
                         weak_ptr_factory_.GetWeakPtr(), client()->GetWeakPtr(),
                         search_terms_args, is_prefetch, original_response,
                         base::TimeTicks::Now()));
}

void ZeroSuggestProvider::Stop(bool clear_cached_results,
                               bool due_to_user_inactivity) {
  if (loader_) {
    LogOmniboxZeroSuggestRequest(ZERO_SUGGEST_REQUEST_INVALIDATED,
                                 /*is_prefetch=*/is_prefetch_loader_);
  }
  loader_.reset();
  is_prefetch_loader_ = false;
  counterfactual_loader_.reset();
  done_ = true;
  result_type_running_ = NONE;

  if (clear_cached_results) {
    // We do not call Clear() on |results_| to retain |verbatim_relevance|
    // value in the |results_| object. |verbatim_relevance| is used at the
    // beginning of the next call to Start() to determine the current url
    // match relevance.
    results_.suggest_results.clear();
    results_.navigation_results.clear();
    results_.experiment_stats.clear();
    results_.headers_map.clear();
    current_query_.clear();
    current_title_.clear();
  }
}

void ZeroSuggestProvider::DeleteMatch(const AutocompleteMatch& match) {
  // Remove the deleted match from the cache, so it is not shown to the user
  // again. Since we cannot remove just one result, blow away the cache.
  //
  // Although the cache is currently only used for REMOTE_NO_URL, we have no
  // easy way of checking the request type after-the-fact. It's safe though, to
  // always clear the cache even if we are on a different request type.
  //
  // TODO(tommycli): It seems quite odd that the cache is saved to a pref, as
  // if we would want to persist it across restarts. That seems to be directly
  // contradictory to the fact that ZeroSuggest results can change rapidly.
  client()->GetPrefs()->SetString(omnibox::kZeroSuggestCachedResults,
                                  std::string());
  BaseSearchProvider::DeleteMatch(match);
}

void ZeroSuggestProvider::AddProviderInfo(ProvidersInfo* provider_info) const {
  BaseSearchProvider::AddProviderInfo(provider_info);
  if (!results_.suggest_results.empty() || !results_.navigation_results.empty())
    provider_info->back().set_times_returned_results_in_session(1);
}

void ZeroSuggestProvider::ResetSession() {
  // The user has started editing in the omnibox, so leave
  // |field_trial_triggered_in_session| unchanged and set
  // |field_trial_triggered| to false since zero suggest is inactive now.
  set_field_trial_triggered(false);
}

ZeroSuggestProvider::ZeroSuggestProvider(AutocompleteProviderClient* client,
                                         AutocompleteProviderListener* listener)
    : BaseSearchProvider(AutocompleteProvider::TYPE_ZERO_SUGGEST, client),
      listener_(listener),
      result_type_running_(NONE) {}

ZeroSuggestProvider::~ZeroSuggestProvider() = default;

const TemplateURL* ZeroSuggestProvider::GetTemplateURL(bool is_keyword) const {
  // Zero suggest provider should not receive keyword results.
  DCHECK(!is_keyword);
  return client()->GetTemplateURLService()->GetDefaultSearchProvider();
}

const AutocompleteInput ZeroSuggestProvider::GetInput(bool is_keyword) const {
  // The callers of this method won't look at the AutocompleteInput's
  // |from_omnibox_focus| member, so we can set its value to false.
  AutocompleteInput input(std::u16string(), current_page_classification_,
                          client()->GetSchemeClassifier());
  input.set_current_url(GURL(current_query_));
  input.set_current_title(current_title_);
  input.set_prevent_inline_autocomplete(true);
  input.set_allow_exact_keyword_match(false);
  return input;
}

bool ZeroSuggestProvider::ShouldAppendExtraParams(
    const SearchSuggestionParser::SuggestResult& result) const {
  // We always use the default provider for search, so append the params.
  return true;
}

void ZeroSuggestProvider::RecordDeletionResult(bool success) {
  if (success) {
    base::RecordAction(
        base::UserMetricsAction("Omnibox.ZeroSuggestDelete.Success"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("Omnibox.ZeroSuggestDelete.Failure"));
  }
}

void ZeroSuggestProvider::OnURLLoadComplete(
    const base::WeakPtr<AutocompleteProviderClient> client,
    TemplateURLRef::SearchTermsArgs search_terms_args,
    bool is_prefetch,
    const std::string& original_response,
    base::TimeTicks request_time,
    const network::SimpleURLLoader* source,
    std::unique_ptr<std::string> response_body) {
  DCHECK(!done_);
  DCHECK_EQ(loader_.get(), source);

  LogOmniboxZeroSuggestRequestRoundTripTime(
      base::TimeTicks::Now() - request_time, is_prefetch);
  LogOmniboxZeroSuggestRequest(ZERO_SUGGEST_RESPONSE_RECEIVED, is_prefetch);
  if (source->LoadedFromCache()) {
    LogOmniboxZeroSuggestRequest(ZERO_SUGGEST_RESPONSE_LOADED_FROM_HTTP_CACHE,
                                 is_prefetch);
  }

  // Issue a follow-up non-cacheable request in the counterfactual arm. The new
  // response is compared against the originally reported cached response to
  // determine its freshness.
  if (OmniboxFieldTrial::kZeroSuggestCacheCounterfactual.Get() && client) {
    // Make sure the request is not cacheable.
    search_terms_args.zero_suggest_cache_duration_sec = 0;

    client->GetRemoteSuggestionsService(/*create_if_necessary=*/true)
        ->CreateSuggestionsRequest(
            search_terms_args, client->GetTemplateURLService(),
            base::BindOnce(&ZeroSuggestProvider::
                               OnRemoteSuggestionsCounterfactualLoaderAvailable,
                           weak_ptr_factory_.GetWeakPtr()),
            base::BindOnce(
                &ZeroSuggestProvider::OnCounterfactualURLLoadComplete,
                weak_ptr_factory_.GetWeakPtr(), is_prefetch,
                original_response));
  }

  const bool response_received =
      response_body && source->NetError() == net::OK &&
      (source->ResponseInfo() && source->ResponseInfo()->headers &&
       source->ResponseInfo()->headers->response_code() == 200);
  const bool results_updated =
      response_received &&
      UpdateResults(SearchSuggestionParser::ExtractJsonData(
          source, std::move(response_body)));
  loader_.reset();
  is_prefetch_loader_ = false;
  done_ = true;
  result_type_running_ = NONE;

  // Do not notify the provider listener for prefetch requests.
  if (!is_prefetch) {
    listener_->OnProviderUpdate(results_updated);
  }
}

void ZeroSuggestProvider::OnCounterfactualURLLoadComplete(
    bool original_is_prefetch,
    const std::string& original_response,
    const network::SimpleURLLoader* source,
    std::unique_ptr<std::string> response) {
  DCHECK(!source->LoadedFromCache());

  if (response && original_response != *response) {
    LogOmniboxZeroSuggestRequest(ZERO_SUGGEST_CACHED_RESPONSE_IS_OUT_OF_DATE,
                                 original_is_prefetch);
  }

  counterfactual_loader_.reset();
}

bool ZeroSuggestProvider::UpdateResults(const std::string& json_data) {
  std::unique_ptr<base::Value> data(
      SearchSuggestionParser::DeserializeJsonData(json_data));
  if (!data)
    return false;

  // Store non-empty response if running the REMOTE_NO_URL variant.
  if (result_type_running_ == REMOTE_NO_URL && !json_data.empty()) {
    client()->GetPrefs()->SetString(omnibox::kZeroSuggestCachedResults,
                                    json_data);

    // If we received an empty result list, we should update the display, as it
    // may be showing cached results that should not be shown.
    //
    // `data->GetListDeprecated()[1]` is the results list.
    const bool non_empty_parsed_list =
        data->is_list() && data->GetListDeprecated().size() >= 2u &&
        data->GetListDeprecated()[1].is_list() &&
        !data->GetListDeprecated()[1].GetListDeprecated().empty();
    const bool non_empty_cache = !results_.suggest_results.empty() ||
                                 !results_.navigation_results.empty();
    if (non_empty_parsed_list && non_empty_cache)
      return false;
  }
  const bool results_updated = ParseSuggestResults(
      *data, kDefaultZeroSuggestRelevance, false, &results_);
  ConvertResultsToAutocompleteMatches();
  return results_updated;
}

AutocompleteMatch ZeroSuggestProvider::NavigationToMatch(
    const SearchSuggestionParser::NavigationResult& navigation) {
  AutocompleteMatch match(this, navigation.relevance(), false,
                          navigation.type());
  match.destination_url = navigation.url();

  match.fill_into_edit +=
      AutocompleteInput::FormattedStringWithEquivalentMeaning(
          navigation.url(), url_formatter::FormatUrl(navigation.url()),
          client()->GetSchemeClassifier(), nullptr);

  // Zero suggest results should always omit protocols and never appear bold.
  auto format_types = AutocompleteMatch::GetFormatTypes(false, false);
  match.contents = url_formatter::FormatUrl(navigation.url(), format_types,
                                            base::UnescapeRule::SPACES, nullptr,
                                            nullptr, nullptr);
  match.contents_class = ClassifyTermMatches({}, match.contents.length(), 0,
                                             ACMatchClassification::URL);

  match.description =
      AutocompleteMatch::SanitizeString(navigation.description());
  match.description_class = ClassifyTermMatches({}, match.description.length(),
                                                0, ACMatchClassification::NONE);

  match.subtypes = navigation.subtypes();
  return match;
}

void ZeroSuggestProvider::OnRemoteSuggestionsLoaderAvailable(
    bool is_prefetch,
    std::unique_ptr<network::SimpleURLLoader> loader) {
  // RemoteSuggestionsService has already started |loader|, so here it's
  // only necessary to grab its ownership until results come in to
  // OnURLLoadComplete().
  loader_ = std::move(loader);
  is_prefetch_loader_ = is_prefetch;
  LogOmniboxZeroSuggestRequest(ZERO_SUGGEST_REQUEST_SENT, is_prefetch);
}

void ZeroSuggestProvider::OnRemoteSuggestionsCounterfactualLoaderAvailable(
    std::unique_ptr<network::SimpleURLLoader> loader) {
  counterfactual_loader_ = std::move(loader);
}

void ZeroSuggestProvider::ConvertResultsToAutocompleteMatches() {
  matches_.clear();

  TemplateURLService* template_url_service = client()->GetTemplateURLService();
  DCHECK(template_url_service);
  const TemplateURL* default_provider =
      template_url_service->GetDefaultSearchProvider();
  // Fail if we can't set the clickthrough URL for query suggestions.
  if (default_provider == nullptr ||
      !default_provider->SupportsReplacement(
          template_url_service->search_terms_data()))
    return;

  MatchMap map;

  // Add all the SuggestResults to the map. We display all ZeroSuggest search
  // suggestions as unbolded.
  for (size_t i = 0; i < results_.suggest_results.size(); ++i) {
    AddMatchToMap(results_.suggest_results[i], std::string(), i, false, false,
                  &map);
  }

  const int num_query_results = map.size();
  const int num_nav_results = results_.navigation_results.size();
  const int num_results = num_query_results + num_nav_results;
  UMA_HISTOGRAM_COUNTS_1M("ZeroSuggest.QueryResults", num_query_results);
  UMA_HISTOGRAM_COUNTS_1M("ZeroSuggest.URLResults", num_nav_results);
  UMA_HISTOGRAM_COUNTS_1M("ZeroSuggest.AllResults", num_results);

  if (num_results == 0)
    return;

  for (MatchMap::const_iterator it(map.begin()); it != map.end(); ++it)
    matches_.push_back(it->second);

  const SearchSuggestionParser::NavigationResults& nav_results(
      results_.navigation_results);
  for (const auto& nav_result : nav_results) {
    matches_.push_back(NavigationToMatch(nav_result));
  }
}

AutocompleteMatch ZeroSuggestProvider::MatchForCurrentText() {
  // The placeholder suggestion for the current URL has high relevance so
  // that it is in the first suggestion slot and inline autocompleted. It
  // gets dropped as soon as the user types something.
  AutocompleteInput tmp(GetInput(false));
  tmp.UpdateText(permanent_text_, std::u16string::npos, tmp.parts());
  const std::u16string description =
      (base::FeatureList::IsEnabled(omnibox::kDisplayTitleForCurrentUrl))
          ? current_title_
          : std::u16string();

  // We pass a nullptr as the |history_url_provider| parameter now to force
  // VerbatimMatch to do a classification, since the text can be a search query.
  // TODO(tommycli): Simplify this - probably just bypass VerbatimMatchForURL.
  AutocompleteMatch match =
      VerbatimMatchForURL(this, client(), tmp, GURL(current_query_),
                          description, results_.verbatim_relevance);
  match.provider = this;
  return match;
}

bool ZeroSuggestProvider::AllowZeroSuggestSuggestions(
    const AutocompleteInput& input) const {
  const auto& page_url = input.current_url();
  const auto page_class = input.current_page_classification();
  const auto input_type = input.type();

  if (input.focus_type() == OmniboxFocusType::DEFAULT)
    return false;

  if (client()->IsOffTheRecord())
    return false;

  if (input_type == metrics::OmniboxInputType::EMPTY) {
    // Function that returns whether EMPTY input zero-suggest is allowed.
    auto IsEmptyZeroSuggestAllowed = [&]() {
      if (page_class == metrics::OmniboxEventProto::CHROMEOS_APP_LIST ||
          IsNTPPage(page_class)) {
        return true;
      }

      if (page_class == metrics::OmniboxEventProto::OTHER) {
        return input.focus_type() == OmniboxFocusType::DELETED_PERMANENT_TEXT &&
               base::FeatureList::IsEnabled(
                   omnibox::kClobberTriggersContextualWebZeroSuggest);
      }

      if (IsSearchResultsPage(page_class)) {
        return input.focus_type() == OmniboxFocusType::DELETED_PERMANENT_TEXT &&
               base::FeatureList::IsEnabled(
                   omnibox::kClobberTriggersSRPZeroSuggest);
      }

      return false;
    };

    // Return false if disallowed. Otherwise, proceed down to further checks.
    if (!IsEmptyZeroSuggestAllowed())
      return false;
  }

  // When omnibox contains pre-populated content, only show zero suggest for
  // pages with URLs the user will recognize.
  //
  // This list intentionally does not include items such as ftp: and file:
  // because (a) these do not work on Android and iOS, where most visited
  // zero suggest is launched and (b) on desktop, where contextual zero suggest
  // is running, these types of schemes aren't eligible to be sent to the
  // server to ask for suggestions (and thus in practice we won't display zero
  // suggest for them).
  if (input_type != metrics::OmniboxInputType::EMPTY &&
      !(page_url.is_valid() &&
        ((page_url.scheme() == url::kHttpScheme) ||
         (page_url.scheme() == url::kHttpsScheme) ||
         (page_url.scheme() == url::kAboutScheme) ||
         (page_url.scheme() ==
          client()->GetEmbedderRepresentationOfAboutScheme())))) {
    return false;
  }

  return true;
}

std::string ZeroSuggestProvider::MaybeUseStoredResponse() {
  // Use the stored response only if running the REMOTE_NO_URL variant.
  if (result_type_running_ != REMOTE_NO_URL) {
    return "";
  }

  std::string json_data =
      client()->GetPrefs()->GetString(omnibox::kZeroSuggestCachedResults);
  if (!json_data.empty()) {
    std::unique_ptr<base::Value> data(
        SearchSuggestionParser::DeserializeJsonData(json_data));
    if (data && ParseSuggestResults(*data, kDefaultZeroSuggestRelevance, false,
                                    &results_))
      ConvertResultsToAutocompleteMatches();
  }
  return json_data;
}

// static
ZeroSuggestProvider::ResultType ZeroSuggestProvider::TypeOfResultToRun(
    AutocompleteProviderClient* client,
    const AutocompleteInput& input,
    const GURL& suggest_url) {
  DCHECK(client);
  // Check if the URL can be sent in any suggest request.
  const TemplateURLService* template_url_service =
      client->GetTemplateURLService();
  DCHECK(template_url_service);
  const TemplateURL* default_provider =
      template_url_service->GetDefaultSearchProvider();

  GURL current_url = input.current_url();
  metrics::OmniboxEventProto::PageClassification current_page_classification =
      input.current_page_classification();

  const bool can_send_current_url = CanSendURL(
      current_url, suggest_url, default_provider, current_page_classification,
      template_url_service->search_terms_data(), client, false);
  // Collect metrics on eligibility.
  GURL arbitrary_insecure_url(kArbitraryInsecureUrlString);
  ZeroSuggestEligibility eligibility = ZeroSuggestEligibility::ELIGIBLE;
  if (!can_send_current_url) {
    const bool can_send_ordinary_url =
        CanSendURL(arbitrary_insecure_url, suggest_url, default_provider,
                   current_page_classification,
                   template_url_service->search_terms_data(), client, false);
    eligibility = can_send_ordinary_url
                      ? ZeroSuggestEligibility::URL_INELIGIBLE
                      : ZeroSuggestEligibility::GENERALLY_INELIGIBLE;
  }
  UMA_HISTOGRAM_ENUMERATION(
      kOmniboxZeroSuggestEligibleHistogramName, static_cast<int>(eligibility),
      static_cast<int>(ZeroSuggestEligibility::ELIGIBLE_MAX_VALUE));

  if (current_page_classification == OmniboxEventProto::CHROMEOS_APP_LIST ||
      current_page_classification ==
          OmniboxEventProto::ANDROID_SHORTCUTS_WIDGET) {
    return REMOTE_NO_URL;
  }

  // Contextual Open Web - does NOT include Search Results Page.
  if (current_page_classification == OmniboxEventProto::OTHER &&
      can_send_current_url) {
    if (input.focus_type() == OmniboxFocusType::ON_FOCUS &&
        (base::FeatureList::IsEnabled(
             omnibox::kOnFocusSuggestionsContextualWeb) ||
         base::FeatureList::IsEnabled(
             omnibox::kOnFocusSuggestionsContextualWebOnContent))) {
      return REMOTE_SEND_URL;
    }

    if (input.focus_type() == OmniboxFocusType::DELETED_PERMANENT_TEXT &&
        base::FeatureList::IsEnabled(
            omnibox::kClobberTriggersContextualWebZeroSuggest)) {
      return REMOTE_SEND_URL;
    }
  }

  // Search Results page classification only.
  if (IsSearchResultsPage(current_page_classification) &&
      can_send_current_url) {
    if (input.focus_type() == OmniboxFocusType::ON_FOCUS &&
        base::FeatureList::IsEnabled(
            omnibox::kOnFocusSuggestionsContextualWebAllowSRP)) {
      return REMOTE_SEND_URL;
    }

    if (input.focus_type() == OmniboxFocusType::DELETED_PERMANENT_TEXT &&
        base::FeatureList::IsEnabled(omnibox::kClobberTriggersSRPZeroSuggest)) {
      return REMOTE_SEND_URL;
    }
  }

  // Default to REMOTE_NO_URL on the NTP, if allowed.
  bool check_authentication_state = !base::FeatureList::IsEnabled(
      omnibox::kOmniboxTrendingZeroPrefixSuggestionsOnNTP);
  bool remote_no_url_allowed = RemoteNoUrlSuggestionsAreAllowed(
      client, template_url_service, check_authentication_state);
  if (IsNTPPage(current_page_classification) && remote_no_url_allowed) {
    return REMOTE_NO_URL;
  }

  return NONE;
}
