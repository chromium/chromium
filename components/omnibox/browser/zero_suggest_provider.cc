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
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/escape.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/base_search_provider.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/remote_suggestions_service.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/omnibox_focus_type.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/template_url_service.h"
#include "components/url_formatter/url_formatter.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "url/gurl.h"

using metrics::OmniboxEventProto;
using metrics::OmniboxInputType;

namespace {

// Represents whether ZeroSuggestProvider is allowed to display contextual
// suggestions on focus, and if not, why not.
// These values are written to logs.  New enum values can be added, but existing
// enums must never be renumbered or deleted and reused.
enum class Eligibility {
  kEligible = 0,
  // kURLIneligible would be kEligible except some property of the current URL
  // itself prevents ZeroSuggest from triggering.
  kURLIneligible = 1,
  kGenerallyIneligible = 2,

  kMaxValue = kGenerallyIneligible,
};

// The provider event types recorded as a result of prefetch and non-prefetch
// requests for zero-prefix suggestions. Each event must be logged at most once
// from when the provider is started until it is stopped.
// These values are written to logs. New enum values can be added, but existing
// enums must never be renumbered or deleted and reused.
enum class Event {
  // Cached response was synchronously converted to displayed matches.
  KCachedResponseConvertedToMatches = 0,
  // Remote request was sent.
  kRequestSent = 1,
  // Remote request was invalidated.
  kRequestInvalidated = 2,
  // Remote response was received asynchronously.
  kRemoteResponseReceived = 3,
  // Remote response was cached.
  kRemoteResponseCached = 4,
  // Remote response ended up being converted to displayed matches. This may
  // happen due to an empty displayed result set or an empty remote result set.
  kRemoteResponseConvertedToMatches = 5,

  kMaxValue = kRemoteResponseConvertedToMatches,
};

void LogEvent(Event event,
              ZeroSuggestProvider::ResultType result_type,
              bool is_prefetch) {
  DCHECK_NE(ZeroSuggestProvider::ResultType::NONE, result_type);
  const std::string result_type_string =
      result_type == ZeroSuggestProvider::ResultType::REMOTE_NO_URL
          ? ".NoURL"
          : ".URLBased";
  const std::string request_type_string =
      is_prefetch ? ".Prefetch" : ".NonPrefetch";
  base::UmaHistogramEnumeration(
      "Omnibox.ZeroSuggestProvider" + result_type_string + request_type_string,
      event);
}

// Relevance value to use if it was not set explicitly by the server.
const int kDefaultZeroSuggestRelevance = 100;

// Metric name tracking the omnibox suggestion eligibility.
constexpr char kOmniboxZeroSuggestEligibleHistogramName[] =
    "Omnibox.ZeroSuggest.Eligible.OnFocusV2";

// Returns whether the current URL can be sent in the suggest request and
// records metrics on eligibility.
// This function only applies to REMOTE_SEND_URL variant.
bool AllowRemoteSendURL(const AutocompleteProviderClient* client,
                        const AutocompleteInput& input) {
  const TemplateURLService* template_url_service =
      client->GetTemplateURLService();
  if (!template_url_service) {
    return false;
  }

  // Returns whether sending the given url in the suggest request is possible.
  auto can_send_request_with_url = [&](GURL url) {
    const TemplateURL* default_provider =
        template_url_service->GetDefaultSearchProvider();
    TemplateURLRef::SearchTermsArgs search_terms_args;
    GURL suggest_url = RemoteSuggestionsService::EndpointUrl(
        search_terms_args, template_url_service);
    OmniboxEventProto::PageClassification current_page_classification =
        input.current_page_classification();

    return BaseSearchProvider::CanSendRequestWithURL(
        url, suggest_url, default_provider, current_page_classification,
        template_url_service->search_terms_data(), client,
        /*sending_search_terms=*/false);
  };

  // Find out whether sending a request with the current page url or otherwise
  // any eligible url is possible and log eligibility metrics.
  const GURL kArbitraryInsecureUrl{"http://www.google.com/"};
  Eligibility eligibility = can_send_request_with_url(input.current_url())
                                ? Eligibility::kEligible
                            : can_send_request_with_url(kArbitraryInsecureUrl)
                                ? Eligibility::kURLIneligible
                                : Eligibility::kGenerallyIneligible;
  base::UmaHistogramEnumeration(kOmniboxZeroSuggestEligibleHistogramName,
                                eligibility);

  return eligibility == Eligibility::kEligible;
}

// Return whether a suggest request can be made without sending the current URL.
// This function only applies to REMOTE_NO_URL variant.
bool AllowRemoteNoURL(const AutocompleteProviderClient* client) {
  const TemplateURLService* template_url_service =
      client->GetTemplateURLService();
  if (!template_url_service) {
    return false;
  }

  const TemplateURL* default_provider =
      template_url_service->GetDefaultSearchProvider();
  TemplateURLRef::SearchTermsArgs search_terms_args;
  GURL suggest_url = RemoteSuggestionsService::EndpointUrl(
      search_terms_args, template_url_service);

  const bool allow_remote_no_url = BaseSearchProvider::CanSendRequest(
      suggest_url, default_provider, template_url_service->search_terms_data(),
      client);

  // Zero-suggest on the NTP is allowed only if the user is signed-in. This
  // check is done not for privacy reasons but to prevent signed-out users from
  // querying the server which does not have any suggestions for them.
  bool check_authentication_state = !base::FeatureList::IsEnabled(
      omnibox::kZeroSuggestOnNTPForSignedOutUsers);

  return allow_remote_no_url &&
         (!check_authentication_state || client->IsAuthenticated());
}

// Returns a sanitized copy of |input|. For zero-suggest, input is expected to
// empty, as it is checked against the suggest response which always has an
// empty query. If those don't match, the response is dropped. Ensures the input
// text is empty. However copies over the URL. Zero-suggest on Web/SRP on Mobile
// relies on the URL to be set.
// TODO(crbug.com/1344004): Find out if the other fields also need to be set.
AutocompleteInput GetSanitizedInput(const AutocompleteInput& input,
                                    const AutocompleteProviderClient* client) {
  AutocompleteInput sanitized_input(u"", input.current_page_classification(),
                                    client->GetSchemeClassifier());
  sanitized_input.set_current_url(input.current_url());
  sanitized_input.set_current_title(input.current_title());
  sanitized_input.set_prevent_inline_autocomplete(true);
  sanitized_input.set_allow_exact_keyword_match(false);
  return sanitized_input;
}

}  // namespace

// static
ZeroSuggestProvider::ResultType ZeroSuggestProvider::TypeOfResultToRun(
    const AutocompleteProviderClient* client,
    const AutocompleteInput& input,
    bool bypass_request_eligibility_checks) {
  const auto page_class = input.current_page_classification();
  const auto focus_type = input.focus_type();

  const bool allow_remote_no_url =
      bypass_request_eligibility_checks || AllowRemoteNoURL(client);

  // New Tab Page family.
  if ((BaseSearchProvider::IsNTPPage(page_class) ||
       page_class == OmniboxEventProto::CHROMEOS_APP_LIST) &&
      allow_remote_no_url) {
    if (focus_type == OmniboxFocusType::ON_FOCUS &&
        input.type() == OmniboxInputType::EMPTY) {
      return ZeroSuggestProvider::REMOTE_NO_URL;
    } else if (page_class == OmniboxEventProto::ANDROID_SHORTCUTS_WIDGET &&
               focus_type == OmniboxFocusType::ON_FOCUS &&
               input.type() == OmniboxInputType::URL) {
      return ZeroSuggestProvider::REMOTE_NO_URL;
    }
  }

  const bool allow_remote_send_url =
      bypass_request_eligibility_checks || AllowRemoteSendURL(client, input);

  // Open Web - does NOT include Search Results Page.
  if (page_class == OmniboxEventProto::OTHER && allow_remote_send_url) {
    if (focus_type == OmniboxFocusType::ON_FOCUS &&
        (base::FeatureList::IsEnabled(
            omnibox::kFocusTriggersContextualWebZeroSuggest))) {
      return ZeroSuggestProvider::REMOTE_SEND_URL;
    }
    if (focus_type == OmniboxFocusType::DELETED_PERMANENT_TEXT &&
        input.type() == OmniboxInputType::EMPTY &&
        base::FeatureList::IsEnabled(
            omnibox::kClobberTriggersContextualWebZeroSuggest)) {
      return ZeroSuggestProvider::REMOTE_SEND_URL;
    }
  }

  // Search Results Page.
  if (BaseSearchProvider::IsSearchResultsPage(page_class) &&
      allow_remote_send_url) {
    if (focus_type == OmniboxFocusType::ON_FOCUS &&
        base::FeatureList::IsEnabled(omnibox::kFocusTriggersSRPZeroSuggest)) {
      return ZeroSuggestProvider::REMOTE_SEND_URL;
    }
    if (focus_type == OmniboxFocusType::DELETED_PERMANENT_TEXT &&
        input.type() == OmniboxInputType::EMPTY &&
        base::FeatureList::IsEnabled(omnibox::kClobberTriggersSRPZeroSuggest)) {
      return ZeroSuggestProvider::REMOTE_SEND_URL;
    }
  }

  return ZeroSuggestProvider::NONE;
}

// static
bool ZeroSuggestProvider::AllowZeroPrefixSuggestions(
    const AutocompleteProviderClient* client,
    const AutocompleteInput& input,
    ResultType* result_type_to_run) {
  DCHECK(result_type_to_run);
  *result_type_to_run = ZeroSuggestProvider::NONE;

  if (input.focus_type() == OmniboxFocusType::DEFAULT) {
    return false;
  }

  // Before checking whether the external conditions for sending a request are
  // met, find out whether zero-prefix suggestions are generally allowed in
  // the given context. This is being done for metrics purposes only.
  *result_type_to_run = TypeOfResultToRun(
      client, input, /*bypass_request_eligibility_checks=*/true);
  if (*result_type_to_run == ZeroSuggestProvider::NONE) {
    base::UmaHistogramEnumeration(kOmniboxZeroSuggestEligibleHistogramName,
                                  Eligibility::kGenerallyIneligible);
    return false;
  }

  *result_type_to_run = TypeOfResultToRun(
      client, input, /*bypass_request_eligibility_checks=*/false);
  return *result_type_to_run != ZeroSuggestProvider::NONE;
}

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

void ZeroSuggestProvider::StartPrefetch(const AutocompleteInput& input) {
  TRACE_EVENT0("omnibox", "ZeroSuggestProvider::StartPrefetch");

  ResultType result_type;
  if (!AllowZeroPrefixSuggestions(client(), input, &result_type)) {
    return;
  }

  // Do not start a request if async requests are disallowed.
  if (input.omit_asynchronous_matches()) {
    return;
  }

  if (prefetch_loader_) {
    LogEvent(Event::kRequestInvalidated, result_type, /*is_prefetch=*/true);
  }

  // Create a loader for the request and take ownership of it.
  TemplateURLRef::SearchTermsArgs search_terms_args;
  search_terms_args.page_classification = input.current_page_classification();
  search_terms_args.focus_type = input.focus_type();
  search_terms_args.current_page_url = result_type == REMOTE_SEND_URL
                                           ? input.current_url().spec()
                                           : std::string();
  prefetch_loader_ =
      client()
          ->GetRemoteSuggestionsService(/*create_if_necessary=*/true)
          ->StartSuggestionsRequest(
              search_terms_args, client()->GetTemplateURLService(),
              base::BindOnce(&ZeroSuggestProvider::OnPrefetchURLLoadComplete,
                             weak_ptr_factory_.GetWeakPtr(),
                             GetSanitizedInput(input, client()), result_type));

  LogEvent(Event::kRequestSent, result_type, /*is_prefetch=*/true);
}

void ZeroSuggestProvider::Start(const AutocompleteInput& input,
                                bool minimal_changes) {
  TRACE_EVENT0("omnibox", "ZeroSuggestProvider::Start");
  Stop(true, false);

  result_type_running_ = NONE;
  if (!AllowZeroPrefixSuggestions(client(), input, &result_type_running_)) {
    return;
  }

  input_ = input;
  set_field_trial_triggered(false);
  set_field_trial_triggered_in_session(false);

  // Convert the stored response to |matches_|, if applicable.
  auto response_data = ReadStoredResponse(result_type_running_);
  if (response_data) {
    if (ConvertResponseToAutocompleteMatches(std::move(response_data))) {
      LogEvent(Event::KCachedResponseConvertedToMatches, result_type_running_,
               /*is_prefetch=*/false);
    }
  }

  // Do not start a request if async requests are disallowed.
  if (input.omit_asynchronous_matches()) {
    return;
  }

  done_ = false;

  // Create a loader for the request and take ownership of it.
  TemplateURLRef::SearchTermsArgs search_terms_args;
  search_terms_args.page_classification = input.current_page_classification();
  search_terms_args.focus_type = input.focus_type();
  search_terms_args.current_page_url = result_type_running_ == REMOTE_SEND_URL
                                           ? input.current_url().spec()
                                           : std::string();
  loader_ = client()
                ->GetRemoteSuggestionsService(/*create_if_necessary=*/true)
                ->StartSuggestionsRequest(
                    search_terms_args, client()->GetTemplateURLService(),
                    base::BindOnce(&ZeroSuggestProvider::OnURLLoadComplete,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   result_type_running_));

  LogEvent(Event::kRequestSent, result_type_running_, /*is_prefetch=*/false);
}

void ZeroSuggestProvider::Stop(bool clear_cached_results,
                               bool due_to_user_inactivity) {
  AutocompleteProvider::Stop(clear_cached_results, due_to_user_inactivity);

  if (loader_) {
    LogEvent(Event::kRequestInvalidated, result_type_running_,
             /*is_prefetch=*/false);
    loader_.reset();
  }

  if (clear_cached_results) {
    experiment_stats_v2s_.clear();
  }
}

void ZeroSuggestProvider::DeleteMatch(const AutocompleteMatch& match) {
  // Remove the deleted match from the cache, so it is not shown to the user
  // again. Since we cannot remove just one result, blow away the cache.
  // Although the cache is currently only used for REMOTE_NO_URL, we have no
  // easy way of checking the request type after-the-fact. It's safe though, to
  // always clear the cache even if we are on a different request type.
  client()->GetPrefs()->SetString(omnibox::kZeroSuggestCachedResults,
                                  std::string());
  BaseSearchProvider::DeleteMatch(match);
}

void ZeroSuggestProvider::AddProviderInfo(ProvidersInfo* provider_info) const {
  BaseSearchProvider::AddProviderInfo(provider_info);
  if (!matches().empty())
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
    : BaseSearchProvider(AutocompleteProvider::TYPE_ZERO_SUGGEST, client) {
  AddListener(listener);
}

ZeroSuggestProvider::~ZeroSuggestProvider() = default;

const TemplateURL* ZeroSuggestProvider::GetTemplateURL(bool is_keyword) const {
  // Zero suggest provider should not receive keyword results.
  DCHECK(!is_keyword);
  return client()->GetTemplateURLService()->GetDefaultSearchProvider();
}

const AutocompleteInput ZeroSuggestProvider::GetInput(bool is_keyword) const {
  return GetSanitizedInput(input_, client());
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
    ResultType result_type,
    const network::SimpleURLLoader* source,
    std::unique_ptr<std::string> response_body) {
  DCHECK(!done_);
  DCHECK_EQ(loader_.get(), source);

  std::unique_ptr<base::Value> response_data = nullptr;

  const bool response_received =
      response_body && source->NetError() == net::OK &&
      (source->ResponseInfo() && source->ResponseInfo()->headers &&
       source->ResponseInfo()->headers->response_code() == 200);
  if (response_received) {
    LogEvent(Event::kRemoteResponseReceived, result_type,
             /*is_prefetch=*/false);
    response_data =
        StoreRemoteResponse(SearchSuggestionParser::ExtractJsonData(
                                source, std::move(response_body)),
                            GetInput(/*is_keyword=*/false), result_type,
                            /*is_prefetch=*/false);
  }

  // Convert the response to |matches_|, if applicable.
  if (response_data) {
    if (ConvertResponseToAutocompleteMatches(std::move(response_data))) {
      LogEvent(Event::kRemoteResponseConvertedToMatches, result_type,
               /*is_prefetch=*/false);
    }
  }

  loader_.reset();
  done_ = true;

  // Do not notify the provider listener for prefetch requests.
  NotifyListeners(!!response_data);
}

void ZeroSuggestProvider::OnPrefetchURLLoadComplete(
    const AutocompleteInput& input,
    ResultType result_type,
    const network::SimpleURLLoader* source,
    std::unique_ptr<std::string> response_body) {
  DCHECK_EQ(prefetch_loader_.get(), source);

  const bool response_received =
      response_body && source->NetError() == net::OK &&
      (source->ResponseInfo() && source->ResponseInfo()->headers &&
       source->ResponseInfo()->headers->response_code() == 200);
  if (response_received) {
    LogEvent(Event::kRemoteResponseReceived, result_type, /*is_prefetch=*/true);
    StoreRemoteResponse(SearchSuggestionParser::ExtractJsonData(
                            source, std::move(response_body)),
                        input, result_type,
                        /*is_prefetch=*/true);
  }

  prefetch_loader_.reset();
}

std::unique_ptr<base::Value> ZeroSuggestProvider::StoreRemoteResponse(
    const std::string& response_json,
    const AutocompleteInput& input,
    ResultType result_type,
    bool is_prefetch) {
  if (response_json.empty()) {
    return nullptr;
  }

  auto response_data =
      SearchSuggestionParser::DeserializeJsonData(response_json);
  if (!response_data) {
    return nullptr;
  }

  SearchSuggestionParser::Results results;
  if (!SearchSuggestionParser::ParseSuggestResults(
          *response_data, GetSanitizedInput(input, client()),
          client()->GetSchemeClassifier(), kDefaultZeroSuggestRelevance,
          /*is_keyword_result=*/false, &results)) {
    return nullptr;
  }

  // Store the valid response only if running the REMOTE_NO_URL variant.
  if (result_type == REMOTE_NO_URL) {
    client()->GetPrefs()->SetString(omnibox::kZeroSuggestCachedResults,
                                    response_json);
    LogEvent(Event::kRemoteResponseCached, result_type, is_prefetch);
  }

  // For display stability reasons, update the displayed results with the remote
  // response only if they are empty or if an empty result set is received. In
  // the latter case, the displayed results may no longer be valid to be shown.
  const bool empty_matches = matches().empty();
  const bool empty_results =
      results.suggest_results.empty() && results.navigation_results.empty();
  if (empty_matches || empty_results) {
    return response_data;
  }

  return nullptr;
}

std::unique_ptr<base::Value> ZeroSuggestProvider::ReadStoredResponse(
    ResultType result_type) {
  // Use the stored response only if running the REMOTE_NO_URL variant.
  if (result_type != REMOTE_NO_URL) {
    return nullptr;
  }

  const std::string response_json =
      client()->GetPrefs()->GetString(omnibox::kZeroSuggestCachedResults);
  if (response_json.empty()) {
    return nullptr;
  }

  return SearchSuggestionParser::DeserializeJsonData(response_json);
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

bool ZeroSuggestProvider::ConvertResponseToAutocompleteMatches(
    std::unique_ptr<base::Value> response_data) {
  DCHECK(response_data);

  SearchSuggestionParser::Results results;
  if (!ParseSuggestResults(*response_data, kDefaultZeroSuggestRelevance,
                           /*is_keyword_result=*/false, &results)) {
    return false;
  }

  matches_.clear();
  suggestion_groups_map_.clear();
  experiment_stats_v2s_.clear();

  // Add all the SuggestResults to the map. We display all ZeroSuggest search
  // suggestions as unbolded.
  MatchMap map;
  for (size_t i = 0; i < results.suggest_results.size(); ++i) {
    AddMatchToMap(results.suggest_results[i], std::string(), i, false, false,
                  &map);
  }

  const int num_query_results = map.size();
  const int num_nav_results = results.navigation_results.size();
  const int num_results = num_query_results + num_nav_results;
  base::UmaHistogramCounts1M("ZeroSuggest.QueryResults", num_query_results);
  base::UmaHistogramCounts1M("ZeroSuggest.URLResults", num_nav_results);
  base::UmaHistogramCounts1M("ZeroSuggest.AllResults", num_results);

  if (num_results == 0) {
    return true;
  }

  for (MatchMap::const_iterator it(map.begin()); it != map.end(); ++it) {
    matches_.push_back(it->second);
  }

  const SearchSuggestionParser::NavigationResults& nav_results(
      results.navigation_results);
  for (const auto& nav_result : nav_results) {
    matches_.push_back(NavigationToMatch(nav_result));
  }

  // Update the suggestion groups information from the server response.
  for (const auto& entry : results.suggestion_groups_map) {
    suggestion_groups_map_[entry.first].MergeFrom(entry.second);
  }

  // Update the list of experiment stats from the server response.
  for (const auto& experiment_stats_v2 : results.experiment_stats_v2s) {
    experiment_stats_v2s_.push_back(experiment_stats_v2);
  }

  return true;
}
