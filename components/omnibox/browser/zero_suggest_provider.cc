// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/zero_suggest_provider.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/json/json_string_value_serializer.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/top_sites.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/contextual_suggestions_service.h"
#include "components/omnibox/browser/history_url_provider.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/omnibox/browser/search_provider.h"
#include "components/omnibox/browser/verbatim_match.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/template_url_service.h"
#include "components/url_formatter/url_formatter.h"
#include "components/variations/net/variations_http_headers.h"
#include "net/base/escape.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "url/gurl.h"

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

// TODO(hfung): The histogram code was copied and modified from
// search_provider.cc.  Refactor and consolidate the code.
// We keep track in a histogram how many suggest requests we send, how
// many suggest requests we invalidate (e.g., due to a user typing
// another character), and how many replies we receive.
// These values are written to logs.  New enum values can be added, but existing
// enums must never be renumbered or deleted and reused.
enum ZeroSuggestRequestsHistogramValue {
  ZERO_SUGGEST_REQUEST_SENT = 1,
  ZERO_SUGGEST_REQUEST_INVALIDATED = 2,
  ZERO_SUGGEST_REPLY_RECEIVED = 3,
  ZERO_SUGGEST_MAX_REQUEST_HISTOGRAM_VALUE
};

void LogOmniboxZeroSuggestRequest(
    ZeroSuggestRequestsHistogramValue request_value) {
  UMA_HISTOGRAM_ENUMERATION("Omnibox.ZeroSuggestRequests", request_value,
                            ZERO_SUGGEST_MAX_REQUEST_HISTOGRAM_VALUE);
}

// Relevance value to use if it was not set explicitly by the server.
const int kDefaultZeroSuggestRelevance = 100;

// Used for testing whether zero suggest is ever available.
constexpr char kArbitraryInsecureUrlString[] = "http://www.google.com/";

// If the user is not signed-in or the user does not have Google set up as their
// default search engine, the personalized service is replaced with the most
// visited service.
bool PersonalizedServiceShouldFallBackToMostVisited(
    PrefService* prefs,
    bool is_authenticated,
    const TemplateURLService* template_url_service) {
  if (!is_authenticated)
    return true;

  if (template_url_service == nullptr)
    return false;

  const TemplateURL* default_provider =
      template_url_service->GetDefaultSearchProvider();
  return default_provider == nullptr ||
         default_provider->GetEngineType(
             template_url_service->search_terms_data()) != SEARCH_ENGINE_GOOGLE;
}

}  // namespace

// static
ZeroSuggestProvider* ZeroSuggestProvider::Create(
    AutocompleteProviderClient* client,
    HistoryURLProvider* history_url_provider,
    AutocompleteProviderListener* listener) {
  return new ZeroSuggestProvider(client, history_url_provider, listener);
}

// static
void ZeroSuggestProvider::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(omnibox::kZeroSuggestCachedResults,
                               std::string());
}

void ZeroSuggestProvider::Start(const AutocompleteInput& input,
                                bool minimal_changes) {
  TRACE_EVENT0("omnibox", "ZeroSuggestProvider::Start");
  matches_.clear();
  Stop(true, false);
  if (!input.from_omnibox_focus() || client()->IsOffTheRecord())
    return;

  // Zero suggest is allowed to run in the Chrome OS app_list context
  // with invalid (empty) input.
  if (input.type() == metrics::OmniboxInputType::INVALID &&
      input.current_page_classification() !=
          metrics::OmniboxEventProto::CHROMEOS_APP_LIST)
    return;

  result_type_running_ = NONE;
  set_field_trial_triggered(false);
  set_field_trial_triggered_in_session(false);
  permanent_text_ = input.text();
  current_query_ = input.current_url().spec();
  current_title_ = input.current_title();
  current_page_classification_ = input.current_page_classification();
  current_url_match_ = MatchForCurrentURL();

  GURL suggest_url = ContextualSuggestionsService::ContextualSuggestionsUrl(
      /*current_url=*/"", input, client()->GetTemplateURLService());
  if (!suggest_url.is_valid())
    return;

  result_type_running_ = TypeOfResultToRun(input.current_url(), suggest_url);
  if (result_type_running_ == NONE)
    return;

  done_ = false;

  MaybeUseCachedSuggestions();

  if (result_type_running_ == MOST_VISITED) {
    most_visited_urls_.clear();
    scoped_refptr<history::TopSites> ts = client()->GetTopSites();
    if (!ts) {
      done_ = true;
      result_type_running_ = NONE;
      return;
    }

    ts->GetMostVisitedURLs(
        base::Bind(&ZeroSuggestProvider::OnMostVisitedUrlsAvailable,
                   weak_ptr_factory_.GetWeakPtr(), most_visited_request_num_),
        false);
    return;
  }

  const std::string current_url = result_type_running_ == DEFAULT_SERP_FOR_URL
                                      ? current_query_
                                      : std::string();
  // Create a request for suggestions, routing completion to
  // OnContextualSuggestionsLoaderAvailable.
  client()
      ->GetContextualSuggestionsService(/*create_if_necessary=*/true)
      ->CreateContextualSuggestionsRequest(
          current_url, client()->GetCurrentVisitTimestamp(), input,
          client()->GetTemplateURLService(),
          base::BindOnce(
              &ZeroSuggestProvider::OnContextualSuggestionsLoaderAvailable,
              weak_ptr_factory_.GetWeakPtr()),
          base::BindOnce(
              &ZeroSuggestProvider::OnURLLoadComplete,
              base::Unretained(this) /* this owns SimpleURLLoader */));
}

void ZeroSuggestProvider::Stop(bool clear_cached_results,
                               bool due_to_user_inactivity) {
  if (loader_)
    LogOmniboxZeroSuggestRequest(ZERO_SUGGEST_REQUEST_INVALIDATED);
  loader_.reset();
  auto* contextual_suggestions_service =
      client()->GetContextualSuggestionsService(/*create_if_necessary=*/false);
  // contextual_suggestions_service can be null if in incognito mode.
  if (contextual_suggestions_service != nullptr) {
    contextual_suggestions_service->StopCreatingContextualSuggestionsRequest();
  }
  // TODO(krb): It would allow us to remove some guards if we could also cancel
  // the TopSites::GetMostVisitedURLs request.
  done_ = true;
  result_type_running_ = NONE;
  ++most_visited_request_num_;

  if (clear_cached_results) {
    // We do not call Clear() on |results_| to retain |verbatim_relevance|
    // value in the |results_| object. |verbatim_relevance| is used at the
    // beginning of the next call to Start() to determine the current url
    // match relevance.
    results_.suggest_results.clear();
    results_.navigation_results.clear();
    current_query_.clear();
    current_title_.clear();
    most_visited_urls_.clear();
  }
}

void ZeroSuggestProvider::DeleteMatch(const AutocompleteMatch& match) {
  if (OmniboxFieldTrial::InZeroSuggestPersonalizedFieldTrial()) {
    // Remove the deleted match from the cache, so it is not shown to the user
    // again. Since we cannot remove just one result, blow away the cache.
    client()->GetPrefs()->SetString(omnibox::kZeroSuggestCachedResults,
                                    std::string());
  }
  BaseSearchProvider::DeleteMatch(match);
}

void ZeroSuggestProvider::AddProviderInfo(ProvidersInfo* provider_info) const {
  BaseSearchProvider::AddProviderInfo(provider_info);
  if (!results_.suggest_results.empty() ||
      !results_.navigation_results.empty() ||
      !most_visited_urls_.empty())
    provider_info->back().set_times_returned_results_in_session(1);
}

void ZeroSuggestProvider::ResetSession() {
  // The user has started editing in the omnibox, so leave
  // |field_trial_triggered_in_session| unchanged and set
  // |field_trial_triggered| to false since zero suggest is inactive now.
  set_field_trial_triggered(false);
}

ZeroSuggestProvider::ZeroSuggestProvider(
    AutocompleteProviderClient* client,
    HistoryURLProvider* history_url_provider,
    AutocompleteProviderListener* listener)
    : BaseSearchProvider(AutocompleteProvider::TYPE_ZERO_SUGGEST, client),
      history_url_provider_(history_url_provider),
      listener_(listener),
      result_type_running_(NONE),
      weak_ptr_factory_(this) {
  // Record whether contextual zero suggest is possible for this user / profile.
  const TemplateURLService* template_url_service =
      client->GetTemplateURLService();
  // Template URL service can be null in tests.
  if (template_url_service != nullptr) {
    AutocompleteInput empty_input;
    GURL suggest_url = ContextualSuggestionsService::ContextualSuggestionsUrl(
        /*current_url=*/"", /*empty input*/ empty_input, template_url_service);
    // To check whether this is allowed, use an arbitrary insecure (http) URL
    // as the URL we'd want suggestions for.  The value of OTHER as the current
    // page classification is to correspond with that URL.
    UMA_HISTOGRAM_BOOLEAN(
        "Omnibox.ZeroSuggest.Eligible.OnProfileOpen",
        suggest_url.is_valid() &&
            CanSendURL(GURL(kArbitraryInsecureUrlString), suggest_url,
                       template_url_service->GetDefaultSearchProvider(),
                       metrics::OmniboxEventProto::OTHER,
                       template_url_service->search_terms_data(), client));
  }
}

ZeroSuggestProvider::~ZeroSuggestProvider() {
}

const TemplateURL* ZeroSuggestProvider::GetTemplateURL(bool is_keyword) const {
  // Zero suggest provider should not receive keyword results.
  DCHECK(!is_keyword);
  return client()->GetTemplateURLService()->GetDefaultSearchProvider();
}

const AutocompleteInput ZeroSuggestProvider::GetInput(bool is_keyword) const {
  // The callers of this method won't look at the AutocompleteInput's
  // |from_omnibox_focus| member, so we can set its value to false.
  AutocompleteInput input(base::string16(), current_page_classification_,
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
    const network::SimpleURLLoader* source,
    std::unique_ptr<std::string> response_body) {
  DCHECK(!done_);
  DCHECK_EQ(loader_.get(), source);

  LogOmniboxZeroSuggestRequest(ZERO_SUGGEST_REPLY_RECEIVED);

  const bool results_updated =
      response_body && source->NetError() == net::OK &&
      (source->ResponseInfo() && source->ResponseInfo()->headers &&
       source->ResponseInfo()->headers->response_code() == 200) &&
      UpdateResults(SearchSuggestionParser::ExtractJsonData(
          source, std::move(response_body)));
  loader_.reset();
  done_ = true;
  result_type_running_ = NONE;
  ++most_visited_request_num_;
  listener_->OnProviderUpdate(results_updated);
}

bool ZeroSuggestProvider::UpdateResults(const std::string& json_data) {
  std::unique_ptr<base::Value> data(
      SearchSuggestionParser::DeserializeJsonData(json_data));
  if (!data)
    return false;

  // When running the personalized service, we want to store suggestion
  // responses if non-empty.
  if (result_type_running_ == DEFAULT_SERP && !json_data.empty()) {
    client()->GetPrefs()->SetString(omnibox::kZeroSuggestCachedResults,
                                    json_data);

    // If we received an empty result list, we should update the display, as it
    // may be showing cached results that should not be shown.
    const base::ListValue* root_list = nullptr;
    const base::ListValue* results_list = nullptr;
    const bool non_empty_parsed_list = data->GetAsList(&root_list) &&
                                       root_list->GetList(1, &results_list) &&
                                       !results_list->empty();
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

void ZeroSuggestProvider::AddSuggestResultsToMap(
    const SearchSuggestionParser::SuggestResults& results,
    MatchMap* map) {
  for (size_t i = 0; i < results.size(); ++i)
    AddMatchToMap(results[i], std::string(), i, false, false, map);
}

AutocompleteMatch ZeroSuggestProvider::NavigationToMatch(
    const SearchSuggestionParser::NavigationResult& navigation) {
  AutocompleteMatch match(this, navigation.relevance(), false,
                          navigation.type());
  match.destination_url = navigation.url();

  // Zero suggest results should always omit protocols and never appear bold.
  auto format_types = AutocompleteMatch::GetFormatTypes(false, false, false);
  match.contents = url_formatter::FormatUrl(navigation.url(), format_types,
                                            net::UnescapeRule::SPACES, nullptr,
                                            nullptr, nullptr);
  match.fill_into_edit +=
      AutocompleteInput::FormattedStringWithEquivalentMeaning(
          navigation.url(), url_formatter::FormatUrl(navigation.url()),
          client()->GetSchemeClassifier(), nullptr);

  AutocompleteMatch::ClassifyLocationInString(base::string16::npos, 0,
      match.contents.length(), ACMatchClassification::URL,
      &match.contents_class);

  match.description =
      AutocompleteMatch::SanitizeString(navigation.description());
  AutocompleteMatch::ClassifyLocationInString(base::string16::npos, 0,
      match.description.length(), ACMatchClassification::NONE,
      &match.description_class);
  match.subtype_identifier = navigation.subtype_identifier();
  return match;
}

void ZeroSuggestProvider::OnMostVisitedUrlsAvailable(
    size_t orig_request_num,
    const history::MostVisitedURLList& urls) {
  if (result_type_running_ != MOST_VISITED ||
      orig_request_num != most_visited_request_num_) {
    return;
  }
  most_visited_urls_ = urls;
  done_ = true;
  ConvertResultsToAutocompleteMatches();
  result_type_running_ = NONE;
  ++most_visited_request_num_;
  listener_->OnProviderUpdate(true);
}

void ZeroSuggestProvider::OnContextualSuggestionsLoaderAvailable(
    std::unique_ptr<network::SimpleURLLoader> loader) {
  // ContextualSuggestionsService has already started |loader|, so here it's
  // only neccessary to grab its ownership until results come in to
  // OnURLLoadComplete().
  loader_ = std::move(loader);
  LogOmniboxZeroSuggestRequest(ZERO_SUGGEST_REQUEST_SENT);
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
  AddSuggestResultsToMap(results_.suggest_results, &map);

  const int num_query_results = map.size();
  const int num_nav_results = results_.navigation_results.size();
  const int num_results = num_query_results + num_nav_results;
  UMA_HISTOGRAM_COUNTS_1M("ZeroSuggest.QueryResults", num_query_results);
  UMA_HISTOGRAM_COUNTS_1M("ZeroSuggest.URLResults", num_nav_results);
  UMA_HISTOGRAM_COUNTS_1M("ZeroSuggest.AllResults", num_results);

  // Show Most Visited results after ZeroSuggest response is received.
  if (result_type_running_ == MOST_VISITED) {
    if (!current_url_match_.destination_url.is_valid())
      return;
    matches_.push_back(current_url_match_);
    int relevance = 600;
    if (num_results > 0) {
      UMA_HISTOGRAM_COUNTS_1M(
          "Omnibox.ZeroSuggest.MostVisitedResultsCounterfactual",
          most_visited_urls_.size());
    }
    const base::string16 current_query_string16(
        base::ASCIIToUTF16(current_query_));
    for (size_t i = 0; i < most_visited_urls_.size(); i++) {
      const history::MostVisitedURL& url = most_visited_urls_[i];
      SearchSuggestionParser::NavigationResult nav(
          client()->GetSchemeClassifier(), url.url,
          AutocompleteMatchType::NAVSUGGEST, 0, url.title, std::string(), false,
          relevance, true, current_query_string16);
      matches_.push_back(NavigationToMatch(nav));
      --relevance;
    }
    return;
  }

  if (num_results == 0)
    return;

  // Normally |current_url_match_.destination_url| should be valid unless it is
  // under particular page context.
  DCHECK(current_page_classification_ ==
             metrics::OmniboxEventProto::CHROMEOS_APP_LIST ||
         current_url_match_.destination_url.is_valid());
  if (current_url_match_.destination_url.is_valid())
    matches_.push_back(current_url_match_);
  for (MatchMap::const_iterator it(map.begin()); it != map.end(); ++it)
    matches_.push_back(it->second);

  const SearchSuggestionParser::NavigationResults& nav_results(
      results_.navigation_results);
  for (auto it = nav_results.begin(); it != nav_results.end(); ++it) {
    matches_.push_back(NavigationToMatch(*it));
  }
}

AutocompleteMatch ZeroSuggestProvider::MatchForCurrentURL() {
  // The placeholder suggestion for the current URL has high relevance so
  // that it is in the first suggestion slot and inline autocompleted. It
  // gets dropped as soon as the user types something.
  AutocompleteInput tmp(GetInput(false));
  tmp.UpdateText(permanent_text_, base::string16::npos, tmp.parts());
  const base::string16 description =
      (base::FeatureList::IsEnabled(omnibox::kDisplayTitleForCurrentUrl))
          ? current_title_
          : base::string16();
  return VerbatimMatchForURL(client(), tmp, GURL(current_query_), description,
                             history_url_provider_,
                             results_.verbatim_relevance);
}

bool ZeroSuggestProvider::AllowZeroSuggestSuggestions(
    const GURL& current_page_url) const {
  // Don't show zero suggest on the NTP.
  // TODO(hfung): Experiment with showing MostVisited zero suggest on NTP
  // under the conditions described in crbug.com/305366.
  if (IsNTPPage(current_page_classification_))
    return false;

  // Don't run if in incognito mode.
  if (client()->IsOffTheRecord())
    return false;

  if (base::FeatureList::IsEnabled(
          omnibox::kOmniboxPopupShortcutIconsInZeroState)) {
    return false;
  }

  // Only show zero suggest for pages with URLs the user will recognize
  // if it is not running in ChromeOS app_list context.
  // This list intentionally does not include items such as ftp: and file:
  // because (a) these do not work on Android and iOS, where non-contextual
  // zero suggest is launched and (b) on desktop, where contextual zero suggest
  // is running, these types of schemes aren't eligible to be sent to the
  // server to ask for suggestions (and thus in practice we won't display zero
  // suggest for them).
  if (current_page_classification_ !=
          metrics::OmniboxEventProto::CHROMEOS_APP_LIST &&
      (!current_page_url.is_valid() ||
       ((current_page_url.scheme() != url::kHttpScheme) &&
        (current_page_url.scheme() != url::kHttpsScheme) &&
        (current_page_url.scheme() != url::kAboutScheme) &&
        (current_page_url.scheme() !=
         client()->GetEmbedderRepresentationOfAboutScheme()))))
    return false;

  return true;
}

void ZeroSuggestProvider::MaybeUseCachedSuggestions() {
  if (result_type_running_ != DEFAULT_SERP)
    return;

  std::string json_data =
      client()->GetPrefs()->GetString(omnibox::kZeroSuggestCachedResults);
  if (!json_data.empty()) {
    std::unique_ptr<base::Value> data(
        SearchSuggestionParser::DeserializeJsonData(json_data));
    if (data && ParseSuggestResults(*data, kDefaultZeroSuggestRelevance, false,
                                    &results_))
      ConvertResultsToAutocompleteMatches();
  }
}

ZeroSuggestProvider::ResultType ZeroSuggestProvider::TypeOfResultToRun(
    const GURL& current_url,
    const GURL& suggest_url) {
  // Check if the URL can be sent in any suggest request.
  const TemplateURLService* template_url_service =
      client()->GetTemplateURLService();
  DCHECK(template_url_service);
  const TemplateURL* default_provider =
      template_url_service->GetDefaultSearchProvider();
  const bool can_send_current_url = CanSendURL(
      current_url, suggest_url, default_provider, current_page_classification_,
      template_url_service->search_terms_data(), client());

  // Collect metrics on eligibility.
  GURL arbitrary_insecure_url(kArbitraryInsecureUrlString);
  ZeroSuggestEligibility eligibility = ZeroSuggestEligibility::ELIGIBLE;
  if (!can_send_current_url) {
    const bool can_send_ordinary_url =
        CanSendURL(arbitrary_insecure_url, suggest_url, default_provider,
                   current_page_classification_,
                   template_url_service->search_terms_data(), client());
    eligibility = can_send_ordinary_url
                      ? ZeroSuggestEligibility::URL_INELIGIBLE
                      : ZeroSuggestEligibility::GENERALLY_INELIGIBLE;
  }
  UMA_HISTOGRAM_ENUMERATION(
      "Omnibox.ZeroSuggest.Eligible.OnFocus", static_cast<int>(eligibility),
      static_cast<int>(ZeroSuggestEligibility::ELIGIBLE_MAX_VALUE));

  // Check if zero suggestions are allowed in the current context.
  if (!AllowZeroSuggestSuggestions(current_url))
    return NONE;

  if (current_page_classification_ ==
      metrics::OmniboxEventProto::CHROMEOS_APP_LIST) {
    return DEFAULT_SERP;
  }

  if (OmniboxFieldTrial::InZeroSuggestPersonalizedFieldTrial())
    return PersonalizedServiceShouldFallBackToMostVisited(
               client()->GetPrefs(), client()->IsAuthenticated(),
               template_url_service)
               ? MOST_VISITED
               : DEFAULT_SERP;

  if (OmniboxFieldTrial::InZeroSuggestMostVisitedWithoutSerpFieldTrial() &&
      client()
          ->GetTemplateURLService()
          ->IsSearchResultsPageFromDefaultSearchProvider(current_url))
    return NONE;

  if (OmniboxFieldTrial::InZeroSuggestMostVisitedFieldTrial())
    return MOST_VISITED;

  return can_send_current_url ? DEFAULT_SERP_FOR_URL : NONE;
}
