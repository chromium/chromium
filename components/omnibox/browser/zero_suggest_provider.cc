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
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
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
#include "net/base/escape.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
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
// The relevance score for navsuggest tiles.
// Navsuggest tiles should be positioned below the Query Tiles object.
const int kMostVisitedTilesRelevance = 1500;

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
const char ZeroSuggestProvider::kNoneVariant[] = "None";
const char ZeroSuggestProvider::kRemoteNoUrlVariant[] = "RemoteNoUrl";
const char ZeroSuggestProvider::kRemoteSendUrlVariant[] = "RemoteSendUrl";
const char ZeroSuggestProvider::kMostVisitedVariant[] = "MostVisited";

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
  GURL suggest_url = RemoteSuggestionsService::EndpointUrl(
      search_terms_args, client()->GetTemplateURLService());
  if (!suggest_url.is_valid())
    return;

  result_type_running_ = TypeOfResultToRun(client(), input, suggest_url);
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

    ts->GetMostVisitedURLs(base::BindRepeating(
        &ZeroSuggestProvider::OnMostVisitedUrlsAvailable,
        weak_ptr_factory_.GetWeakPtr(), most_visited_request_num_));
    return;
  }

  search_terms_args.current_page_url =
      result_type_running_ == REMOTE_SEND_URL ? current_query_ : std::string();
  // Create a request for suggestions, routing completion to
  // OnRemoteSuggestionsLoaderAvailable.
  client()
      ->GetRemoteSuggestionsService(/*create_if_necessary=*/true)
      ->CreateSuggestionsRequest(
          search_terms_args, client()->GetTemplateURLService(),
          base::BindOnce(
              &ZeroSuggestProvider::OnRemoteSuggestionsLoaderAvailable,
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
    results_.experiment_stats.clear();
    results_.headers_map.clear();
    current_query_.clear();
    current_title_.clear();
    most_visited_urls_.clear();
  }
}

void ZeroSuggestProvider::DeleteMatch(const AutocompleteMatch& match) {
  if (base::Contains(OmniboxFieldTrial::GetZeroSuggestVariants(
                         current_page_classification_),
                     kRemoteNoUrlVariant)) {
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
    AutocompleteProviderListener* listener)
    : BaseSearchProvider(AutocompleteProvider::TYPE_ZERO_SUGGEST, client),
      listener_(listener),
      result_type_running_(NONE) {
  // Record whether remote zero suggest is possible for this user / profile.
  const TemplateURLService* template_url_service =
      client->GetTemplateURLService();
  // Template URL service can be null in tests.
  if (template_url_service != nullptr) {
    GURL suggest_url = RemoteSuggestionsService::EndpointUrl(
        TemplateURLRef::SearchTermsArgs(), template_url_service);
    // To check whether this is allowed, use an arbitrary insecure (http) URL
    // as the URL we'd want suggestions for.  The value of OTHER as the current
    // page classification is to correspond with that URL.
    UMA_HISTOGRAM_BOOLEAN(
        "Omnibox.ZeroSuggest.Eligible.OnProfileOpen",
        suggest_url.is_valid() &&
            CanSendURL(GURL(kArbitraryInsecureUrlString), suggest_url,
                       template_url_service->GetDefaultSearchProvider(),
                       metrics::OmniboxEventProto::OTHER,
                       template_url_service->search_terms_data(), client,
                       false));
  }
}

ZeroSuggestProvider::~ZeroSuggestProvider() = default;

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

  // When running the REMOTE_NO_URL variant, we want to store suggestion
  // responses if non-empty.
  if (base::FeatureList::IsEnabled(omnibox::kOmniboxZeroSuggestCaching) &&
      result_type_running_ == REMOTE_NO_URL && !json_data.empty()) {
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
                                            net::UnescapeRule::SPACES, nullptr,
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

void ZeroSuggestProvider::OnRemoteSuggestionsLoaderAvailable(
    std::unique_ptr<network::SimpleURLLoader> loader) {
  // RemoteSuggestionsService has already started |loader|, so here it's
  // only necessary to grab its ownership until results come in to
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

  // Show Most Visited results after ZeroSuggest response is received.
  if (result_type_running_ == MOST_VISITED) {
    // Ensure we don't show most visited URL suggestions on NTP.
    // This allows us to prevent undesired side outcome of presenting
    // URL suggestions to users who are not in the personalized field trial for
    // zero query suggestions.
    if (IsNTPPage(current_page_classification_) ||
        !current_text_match_.destination_url.is_valid()) {
      return;
    }
    matches_.push_back(current_text_match_);

    // Short-circuit in case we have no MOST_VISITED urls to show.
    if (most_visited_urls_.empty())
      return;

    if (base::FeatureList::IsEnabled(omnibox::kMostVisitedTiles)) {
      AutocompleteMatch match =
          NavigationToMatch(SearchSuggestionParser::NavigationResult(
              client()->GetSchemeClassifier(), GURL::EmptyGURL(),
              AutocompleteMatchType::TILE_NAVSUGGEST, {}, base::string16(),
              std::string(), false, kMostVisitedTilesRelevance, true,
              base::ASCIIToUTF16(current_query_)));
      match.navsuggest_tiles.reserve(most_visited_urls_.size());

      for (const auto& url : most_visited_urls_) {
        match.navsuggest_tiles.push_back({url.url, url.title});
      }
      matches_.push_back(std::move(match));
    } else {
      int relevance = 600;
      for (const auto& url : most_visited_urls_) {
        SearchSuggestionParser::NavigationResult nav(
            client()->GetSchemeClassifier(), url.url,
            AutocompleteMatchType::NAVSUGGEST, {}, url.title, std::string(),
            false, relevance, true, base::ASCIIToUTF16(current_query_));
        matches_.push_back(NavigationToMatch(nav));
        --relevance;
      }
    }
    return;
  }

  if (num_results == 0)
    return;

#if defined(OS_ANDROID) || defined(OS_IOS)
  // Android needs the verbatim match on non-NTP surfaces to properly present
  // the Search Ready Omnibox URL edit widget. Desktop specifically does NOT
  // want to show verbatim matches in remotely-fetched ZeroSuggest anymore.
  // iOS we are keeping the same as Android for now. No strong reason to change.
  if (!IsNTPPage(current_page_classification_) &&
      current_text_match_.destination_url.is_valid()) {
    matches_.push_back(current_text_match_);
  }
#endif

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
  tmp.UpdateText(permanent_text_, base::string16::npos, tmp.parts());
  const base::string16 description =
      (base::FeatureList::IsEnabled(omnibox::kDisplayTitleForCurrentUrl))
          ? current_title_
          : base::string16();

  // We pass a nullptr as the |history_url_provider| parameter now to force
  // VerbatimMatch to do a classification, since the text can be a search query.
  // TODO(tommycli): Simplify this - probably just bypass VerbatimMatchForURL.
  AutocompleteMatch match = VerbatimMatchForURL(
      client(), tmp, GURL(current_query_), description,
      /*history_url_provider=*/nullptr, results_.verbatim_relevance);
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

void ZeroSuggestProvider::MaybeUseCachedSuggestions() {
  if (!base::FeatureList::IsEnabled(omnibox::kOmniboxZeroSuggestCaching) ||
      result_type_running_ != REMOTE_NO_URL) {
    return;
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

  const auto field_trial_variants =
      OmniboxFieldTrial::GetZeroSuggestVariants(current_page_classification);

  if (base::Contains(field_trial_variants, kNoneVariant))
    return NONE;

  if (current_page_classification == OmniboxEventProto::CHROMEOS_APP_LIST)
    return REMOTE_NO_URL;

  // Contextual Open Web - (same client side behavior for multiple variants).
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

  // Reactive Zero-Prefix Suggestions (rZPS) on NTP cases.
  bool check_authentication_state = !base::FeatureList::IsEnabled(
      omnibox::kOmniboxTrendingZeroPrefixSuggestionsOnNTP);
  bool remote_no_url_allowed = RemoteNoUrlSuggestionsAreAllowed(
      client, template_url_service, check_authentication_state);
  if (remote_no_url_allowed) {
    // NTP Omnibox.
    if ((current_page_classification == OmniboxEventProto::NTP ||
         current_page_classification ==
             OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS) &&
        base::FeatureList::IsEnabled(
            omnibox::kReactiveZeroSuggestionsOnNTPOmnibox)) {
      return REMOTE_NO_URL;
    }
    // NTP Realbox.
    if (current_page_classification == OmniboxEventProto::NTP_REALBOX &&
        base::FeatureList::IsEnabled(
            omnibox::kReactiveZeroSuggestionsOnNTPRealbox)) {
      return REMOTE_NO_URL;
    }
  }

  if (base::Contains(field_trial_variants, kRemoteNoUrlVariant) &&
      remote_no_url_allowed) {
    return REMOTE_NO_URL;
  }

  if (base::Contains(field_trial_variants, kRemoteSendUrlVariant) &&
      can_send_current_url)
    return REMOTE_SEND_URL;

  if (base::Contains(field_trial_variants, kMostVisitedVariant))
    return MOST_VISITED;

#if !defined(OS_IOS)
  // For Desktop and Android, default to REMOTE_NO_URL on the NTP, if allowed.
  if (IsNTPPage(current_page_classification) && remote_no_url_allowed)
    return REMOTE_NO_URL;
#endif

#if defined(OS_ANDROID) || defined(OS_IOS)
  // For Android and iOS, default to MOST_VISITED everywhere except on the SERP.
  if (!IsSearchResultsPage(current_page_classification)) {
    return MOST_VISITED;
  }
#endif

  return NONE;
}
