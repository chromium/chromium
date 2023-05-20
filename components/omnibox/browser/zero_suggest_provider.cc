// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/zero_suggest_provider.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/strings/escape.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/omnibox_triggered_feature_service.h"
#include "components/omnibox/browser/page_classification_functions.h"
#include "components/omnibox/browser/remote_suggestions_service.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/omnibox/browser/zero_suggest_cache_service.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/template_url_service.h"
#include "components/url_formatter/url_formatter.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "url/gurl.h"

using OEP = metrics::OmniboxEventProto;
using OFT = metrics::OmniboxFocusType;
using OIT = metrics::OmniboxInputType;

namespace {

using ResultType = ZeroSuggestProvider::ResultType;

// Represents whether ZeroSuggestProvider is allowed to display zero-prefix
// suggestions, and if not, why not.
// These values are written to logs.  New enum values can be added, but existing
// enums must never be renumbered or deleted and reused.
enum class Eligibility {
  kEligible = 0,
  // Suggest request without sending the current page URL cannot be made. E.g.,
  // the user is in incognito mode or Google is not set as the default search
  // provider.
  kRequestNoURLIneligible = 1,
  // Suggest request with sending the current page URL cannot be made. E.g.,
  // the user has not consented and the page is not the SRP associated with the
  // default search provider.
  kRequestSendURLIneligible = 2,
  // Zero-prefix suggestions are not eligible in the given context. E.g., due to
  // the page classification, focus type, input type, or an invalid page URL.
  kGenerallyIneligible = 3,

  kMaxValue = kGenerallyIneligible,
};

// The provider event types recorded as a result of prefetch and non-prefetch
// requests for zero-prefix suggestions. Each event must be logged at most once
// from when the provider is started until it is stopped.
// These values are written to logs. New enum values can be added, but existing
// enums must never be renumbered or deleted and reused.
enum class Event {
  // Cached response was synchronously converted to displayed matches.
  // Recorded for non-prefetch requests only.
  kCachedResponseConvertedToMatches = 0,
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
  // Recorded for non-prefetch requests only.
  kRemoteResponseConvertedToMatches = 5,

  kMaxValue = kRemoteResponseConvertedToMatches,
};

void LogEvent(const Event event,
              const ResultType result_type,
              const bool is_prefetch) {
  DCHECK_NE(ResultType::kNone, result_type);

  const std::string result_type_string =
      result_type == ResultType::kRemoteNoURL ? ".NoURL" : ".URLBased";
  const std::string request_type_string =
      is_prefetch ? ".Prefetch" : ".NonPrefetch";
  base::UmaHistogramEnumeration(
      "Omnibox.ZeroSuggestProvider" + result_type_string + request_type_string,
      event);
}

// Relevance value to use if it was not set explicitly by the server.
const int kDefaultZeroSuggestRelevance = 100;

// Return whether a suggest request can be made with sending the current URL.
// This function only applies to kRemoteSendURL variant.
bool AllowRemoteSendURL(const AutocompleteProviderClient* client,
                        const AutocompleteInput& input) {
  const TemplateURLService* template_url_service =
      client->GetTemplateURLService();
  if (!template_url_service) {
    return false;
  }
  const TemplateURL* default_provider =
      template_url_service->GetDefaultSearchProvider();
  if (!default_provider) {
    return false;
  }

  // Explicitly test the conditions for sending a suggest request without
  // sending the current URL are also met in case these two tests diverge.
  return BaseSearchProvider::CanSendZeroSuggestRequest(
             default_provider, template_url_service->search_terms_data(),
             client) &&
         BaseSearchProvider::CanSendSuggestRequestWithURL(
             input.current_url(), default_provider,
             template_url_service->search_terms_data(), client);
}

// Return whether a suggest request can be made without sending the current URL.
// This function only applies to kRemoteNoURL variant.
bool AllowRemoteNoURL(const AutocompleteProviderClient* client) {
  const TemplateURLService* template_url_service =
      client->GetTemplateURLService();
  if (!template_url_service) {
    return false;
  }
  const TemplateURL* default_provider =
      template_url_service->GetDefaultSearchProvider();
  if (!default_provider) {
    return false;
  }

  const bool allow_remote_no_url =
      BaseSearchProvider::CanSendZeroSuggestRequest(
          default_provider, template_url_service->search_terms_data(), client);

  // Zero-suggest on the NTP is allowed only if the user is signed-in. This
  // check is done not for privacy reasons but to prevent signed-out users from
  // querying the server which does not have any suggestions for them.
  bool check_authentication_state = !base::FeatureList::IsEnabled(
      omnibox::kZeroSuggestOnNTPForSignedOutUsers);

  return allow_remote_no_url &&
         (!check_authentication_state || client->IsAuthenticated());
}

// Returns a copy of |input| with an empty text for zero-suggest. The input text
// is checked against the suggest response which always has an empty query. If
// those don't match, the response is dropped. It however copies over the URL,
// as zero-suggest on Web/SRP on Mobile relies on the URL to be set.
// TODO(crbug.com/1344004): Find out if the other fields also need to be set and
// whether this call can be avoided altogether by e.g., not checking the input
// text against the query in the response.
AutocompleteInput GetZeroSuggestInput(
    const AutocompleteInput& input,
    const AutocompleteProviderClient* client) {
  AutocompleteInput sanitized_input(u"", input.current_page_classification(),
                                    client->GetSchemeClassifier());
  sanitized_input.set_current_url(input.current_url());
  sanitized_input.set_current_title(input.current_title());
  sanitized_input.set_prevent_inline_autocomplete(true);
  sanitized_input.set_allow_exact_keyword_match(false);
  return sanitized_input;
}

// Called in StoreRemoteResponse() and ReadStoredResponse() to determine if the
// zero suggest cache is being used to store ZPS responses received from the
// remote Suggest service for the given |result_type|.
bool ShouldCacheResultTypeInContext(const ResultType result_type,
                                    const OEP::PageClassification page_class) {
  switch (result_type) {
    case ResultType::kRemoteNoURL:
      return true;
    case ResultType::kRemoteSendURL:
      return omnibox::IsSearchResultsPage(page_class)
                 ? base::FeatureList::IsEnabled(
                       omnibox::kZeroSuggestPrefetchingOnSRP)
                 : base::FeatureList::IsEnabled(
                       omnibox::kZeroSuggestPrefetchingOnWeb);
    case ResultType::kNone:
      NOTREACHED() << "kNone is not a valid zero suggest result type.";
      return false;
  }
}

// Called in OnURLLoadComplete() or OnPrefetchURLLoadComplete() when the
// remote response is received with the input for which the request was made.
//
// Populates |results| with the response if it can be successfully parsed for
// |input|; and stores the response json in the user prefs, if applicable to
// |result_type|. Returns true if the response can be successfully parsed.
bool StoreRemoteResponse(const std::string& response_json,
                         AutocompleteProviderClient* client,
                         const AutocompleteInput& input,
                         const ResultType result_type,
                         const bool is_prefetch,
                         SearchSuggestionParser::Results* results) {
  DCHECK(results);
  DCHECK_NE(ResultType::kNone, result_type);

  if (response_json.empty()) {
    return false;
  }

  auto response_data =
      SearchSuggestionParser::DeserializeJsonData(response_json);
  if (!response_data) {
    return false;
  }

  if (!SearchSuggestionParser::ParseSuggestResults(
          *response_data, input, client->GetSchemeClassifier(),
          kDefaultZeroSuggestRelevance,
          /*is_keyword_result=*/false, results)) {
    return false;
  }

  const auto page_class = input.current_page_classification();
  if (!ShouldCacheResultTypeInContext(result_type, page_class)) {
    return true;
  }

  // Force use of empty page URL when given an input with "No URL" result type.
  const std::string page_url = result_type != ResultType::kRemoteNoURL
                                   ? input.current_url().spec()
                                   : std::string();

  if (base::FeatureList::IsEnabled(omnibox::kZeroSuggestInMemoryCaching)) {
    auto* zero_suggest_cache_service = client->GetZeroSuggestCacheService();
    if (zero_suggest_cache_service != nullptr) {
      zero_suggest_cache_service->StoreZeroSuggestResponse(page_url,
                                                           response_json);
      LogEvent(Event::kRemoteResponseCached, result_type, is_prefetch);
    }
  } else {
    omnibox::SetUserPreferenceForZeroSuggestCachedResponse(
        client->GetPrefs(), page_url, response_json);
    LogEvent(Event::kRemoteResponseCached, result_type, is_prefetch);
  }

  return true;
}

// Called in Start() with an input ensured to be appropriate for zero-suggest.
//
// Returns true if the response stored in the user prefs is applicable to
// |result_type| and can be successfully parsed for |input|. If so, populates
// |results| with the stored response.
bool ReadStoredResponse(const AutocompleteProviderClient* client,
                        const AutocompleteInput& input,
                        const ResultType result_type,
                        SearchSuggestionParser::Results* results) {
  DCHECK(results);
  DCHECK_NE(ResultType::kNone, result_type);

  const auto page_class = input.current_page_classification();
  if (!ShouldCacheResultTypeInContext(result_type, page_class)) {
    return false;
  }

  std::string response_json;
  // Force use of empty page URL when given an input with "No URL" result type.
  const std::string page_url = result_type != ResultType::kRemoteNoURL
                                   ? input.current_url().spec()
                                   : std::string();

  if (base::FeatureList::IsEnabled(omnibox::kZeroSuggestInMemoryCaching)) {
    auto* zero_suggest_cache_service = client->GetZeroSuggestCacheService();
    if (zero_suggest_cache_service != nullptr) {
      ZeroSuggestCacheService::CacheEntry entry =
          zero_suggest_cache_service->ReadZeroSuggestResponse(page_url);
      response_json = entry.response_json;
    }
  } else {
    response_json = omnibox::GetUserPreferenceForZeroSuggestCachedResponse(
        client->GetPrefs(), page_url);
  }

  if (response_json.empty()) {
    return false;
  }

  absl::optional<base::Value::List> response_data =
      SearchSuggestionParser::DeserializeJsonData(response_json);
  if (!response_data) {
    return false;
  }

  if (!SearchSuggestionParser::ParseSuggestResults(
          *response_data, input, client->GetSchemeClassifier(),
          kDefaultZeroSuggestRelevance,
          /*is_keyword_result=*/false, results)) {
    return false;
  }

  return true;
}

}  // namespace

// static
ZeroSuggestProvider::ResultType ZeroSuggestProvider::ResultTypeToRun(
    const AutocompleteInput& input) {
  const auto page_class = input.current_page_classification();
  const auto focus_type_input_type =
      std::make_pair(input.focus_type(), input.type());

  // Android Search Widget.
  if (page_class == OEP::ANDROID_SHORTCUTS_WIDGET) {
    if (focus_type_input_type ==
        std::make_pair(OFT::INTERACTION_FOCUS, OIT::URL)) {
      return ResultType::kRemoteNoURL;
    }
  }

  // New Tab Page.
  if (omnibox::IsNTPPage(page_class)) {
    if (focus_type_input_type ==
        std::make_pair(OFT::INTERACTION_FOCUS, OIT::EMPTY)) {
      return ResultType::kRemoteNoURL;
    }
  }

  // The following cases require sending the current page URL in the request.
  // Ensure the URL is valid with an HTTP(S) scheme and is not the NTP page URL.
  if (omnibox::IsNTPPage(page_class) ||
      !BaseSearchProvider::CanSendPageURLInRequest(input.current_url())) {
    return ResultType::kNone;
  }

  // Open Web - does NOT include Search Results Page.
  if (omnibox::IsOtherWebPage(page_class)) {
    if (focus_type_input_type ==
            std::make_pair(OFT::INTERACTION_FOCUS, OIT::URL) &&
        base::FeatureList::IsEnabled(
            omnibox::kFocusTriggersContextualWebZeroSuggest)) {
      return ResultType::kRemoteSendURL;
    }
    if (focus_type_input_type ==
            std::make_pair(OFT::INTERACTION_CLOBBER, OIT::EMPTY) &&
        base::FeatureList::IsEnabled(
            omnibox::kClobberTriggersContextualWebZeroSuggest)) {
      return ResultType::kRemoteSendURL;
    }
  }

  // Search Results Page.
  if (omnibox::IsSearchResultsPage(page_class)) {
    if (focus_type_input_type ==
            std::make_pair(OFT::INTERACTION_FOCUS, OIT::URL) &&
        base::FeatureList::IsEnabled(omnibox::kFocusTriggersSRPZeroSuggest)) {
      return ResultType::kRemoteSendURL;
    }
    if (focus_type_input_type ==
            std::make_pair(OFT::INTERACTION_CLOBBER, OIT::EMPTY) &&
        base::FeatureList::IsEnabled(omnibox::kClobberTriggersSRPZeroSuggest)) {
      return ResultType::kRemoteSendURL;
    }
  }

  return ResultType::kNone;
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
  registry->RegisterDictionaryPref(omnibox::kZeroSuggestCachedResultsWithURL);
}

bool ZeroSuggestProvider::AllowZeroPrefixSuggestions(
    const AutocompleteProviderClient* client,
    const AutocompleteInput& input) {
  auto eligibility = Eligibility::kEligible;

  switch (ResultTypeToRun(input)) {
    case ResultType::kRemoteNoURL: {
      if (!AllowRemoteNoURL(client)) {
        eligibility = Eligibility::kRequestNoURLIneligible;
      }
      break;
    }
    case ResultType::kRemoteSendURL: {
      if (!AllowRemoteSendURL(client, input)) {
        eligibility = Eligibility::kRequestSendURLIneligible;
      }
      break;
    }
    default: {
      eligibility = Eligibility::kGenerallyIneligible;
      break;
    }
  }

  base::UmaHistogramEnumeration("Omnibox.ZeroSuggestProvider.Eligibility",
                                eligibility);
  return eligibility == Eligibility::kEligible;
}

void ZeroSuggestProvider::StartPrefetch(const AutocompleteInput& input) {
  AutocompleteProvider::StartPrefetch(input);

  TRACE_EVENT0("omnibox", "ZeroSuggestProvider::StartPrefetch");

  if (!AllowZeroPrefixSuggestions(client(), input)) {
    return;
  }

  const ResultType result_type = ResultTypeToRun(input);
  if (result_type == ResultType::kNone) {
    return;
  }

  if (prefetch_loader_) {
    LogEvent(Event::kRequestInvalidated, result_type, /*is_prefetch=*/true);
  }

  TemplateURLRef::SearchTermsArgs search_terms_args;
  search_terms_args.page_classification = input.current_page_classification();
  search_terms_args.focus_type = input.focus_type();
  search_terms_args.current_page_url = result_type == ResultType::kRemoteSendURL
                                           ? input.current_url().spec()
                                           : std::string();

  // AllowZeroPrefixSuggestions() ensures these are not nullptr.
  const TemplateURLService* template_url_service =
      client()->GetTemplateURLService();
  const TemplateURL* template_url =
      template_url_service->GetDefaultSearchProvider();

  // Create a loader for the request and take ownership of it.
  prefetch_loader_ =
      client()
          ->GetRemoteSuggestionsService(/*create_if_necessary=*/true)
          ->StartZeroPrefixSuggestionsRequest(
              template_url, search_terms_args,
              template_url_service->search_terms_data(),
              base::BindOnce(&ZeroSuggestProvider::OnPrefetchURLLoadComplete,
                             weak_ptr_factory_.GetWeakPtr(),
                             GetZeroSuggestInput(input, client()),
                             result_type));

  LogEvent(Event::kRequestSent, result_type, /*is_prefetch=*/true);
}

void ZeroSuggestProvider::Start(const AutocompleteInput& input,
                                bool minimal_changes) {
  TRACE_EVENT0("omnibox", "ZeroSuggestProvider::Start");
  Stop(true, false);

  if (!AllowZeroPrefixSuggestions(client(), input)) {
    return;
  }

  result_type_running_ = ResultTypeToRun(input);
  if (result_type_running_ == ResultType::kNone) {
    return;
  }

  // Convert the stored response to |matches_|, if applicable.
  SearchSuggestionParser::Results results;
  if (ReadStoredResponse(client(), GetZeroSuggestInput(input, client()),
                         result_type_running_, &results)) {
    ConvertSuggestResultsToAutocompleteMatches(results, input);
    LogEvent(Event::kCachedResponseConvertedToMatches, result_type_running_,
             /*is_prefetch=*/false);
  }

  // Do not start a request if async requests are disallowed.
  if (input.omit_asynchronous_matches()) {
    return;
  }

  done_ = false;

  // AllowZeroPrefixSuggestions() ensures these are not nullptr.
  const TemplateURLService* template_url_service =
      client()->GetTemplateURLService();
  const TemplateURL* template_url =
      template_url_service->GetDefaultSearchProvider();

  TemplateURLRef::SearchTermsArgs search_terms_args;
  search_terms_args.page_classification = input.current_page_classification();
  search_terms_args.focus_type = input.focus_type();
  search_terms_args.current_page_url =
      result_type_running_ == ResultType::kRemoteSendURL
          ? input.current_url().spec()
          : std::string();

  // Create a loader for the request and take ownership of it.
  loader_ = client()
                ->GetRemoteSuggestionsService(/*create_if_necessary=*/true)
                ->StartZeroPrefixSuggestionsRequest(
                    template_url, search_terms_args,
                    template_url_service->search_terms_data(),
                    base::BindOnce(&ZeroSuggestProvider::OnURLLoadComplete,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   GetZeroSuggestInput(input, client()),
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
  result_type_running_ = ResultType::kNone;

  if (clear_cached_results) {
    experiment_stats_v2s_.clear();
  }
}

void ZeroSuggestProvider::DeleteMatch(const AutocompleteMatch& match) {
  // Remove the deleted match from the cache, so it is not shown to the user
  // again. Since the deleted result might have been surfaced as a suggestion on
  // both NTP and SRP/Web, blow away the entire cache.
  if (base::FeatureList::IsEnabled(omnibox::kZeroSuggestInMemoryCaching)) {
    auto* zero_suggest_cache_service = client()->GetZeroSuggestCacheService();
    if (zero_suggest_cache_service) {
      zero_suggest_cache_service->ClearCache();
    }
  } else {
    client()->GetPrefs()->SetString(omnibox::kZeroSuggestCachedResults,
                                    std::string());
    client()->GetPrefs()->SetDict(omnibox::kZeroSuggestCachedResultsWithURL,
                                  base::Value::Dict());
  }

  BaseSearchProvider::DeleteMatch(match);
}

void ZeroSuggestProvider::AddProviderInfo(ProvidersInfo* provider_info) const {
  BaseSearchProvider::AddProviderInfo(provider_info);
  if (!matches().empty())
    provider_info->back().set_times_returned_results_in_session(1);
}

ZeroSuggestProvider::ZeroSuggestProvider(AutocompleteProviderClient* client,
                                         AutocompleteProviderListener* listener)
    : BaseSearchProvider(AutocompleteProvider::TYPE_ZERO_SUGGEST, client) {
  AddListener(listener);
}

ZeroSuggestProvider::~ZeroSuggestProvider() = default;

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
    const AutocompleteInput& input,
    const ResultType result_type,
    const network::SimpleURLLoader* source,
    const bool response_received,
    std::unique_ptr<std::string> response_body) {
  TRACE_EVENT0("omnibox", "ZeroSuggestProvider::OnURLLoadComplete");

  DCHECK(!done_);
  DCHECK_EQ(loader_.get(), source);

  if (!response_received) {
    loader_.reset();
    done_ = true;
    return;
  }

  LogEvent(Event::kRemoteResponseReceived, result_type,
           /*is_prefetch=*/false);

  SearchSuggestionParser::Results results;
  const bool response_parsed = StoreRemoteResponse(
      SearchSuggestionParser::ExtractJsonData(source, std::move(response_body)),
      client(), input, result_type,
      /*is_prefetch=*/false, &results);
  if (!response_parsed) {
    loader_.reset();
    done_ = true;
    return;
  }

  loader_.reset();
  done_ = true;

  // For display stability reasons, update the displayed results with the remote
  // response only if they are empty or if an empty result set is received. In
  // the latter case, the displayed results may no longer be valid to be shown.
  const bool empty_matches = matches().empty();
  const bool empty_results =
      results.suggest_results.empty() && results.navigation_results.empty();
  if (!empty_matches && !empty_results) {
    return;
  }

  // Convert the response to |matches_| and notify the listeners.
  ConvertSuggestResultsToAutocompleteMatches(results, input);
  LogEvent(Event::kRemoteResponseConvertedToMatches, result_type,
           /*is_prefetch=*/false);
  NotifyListeners(/*updated_matches=*/true);
}

void ZeroSuggestProvider::OnPrefetchURLLoadComplete(
    const AutocompleteInput& input,
    const ResultType result_type,
    const network::SimpleURLLoader* source,
    const bool response_received,
    std::unique_ptr<std::string> response_body) {
  TRACE_EVENT0("omnibox", "ZeroSuggestProvider::OnPrefetchURLLoadComplete");

  DCHECK_EQ(prefetch_loader_.get(), source);

  if (response_received) {
    LogEvent(Event::kRemoteResponseReceived, result_type, /*is_prefetch=*/true);

    SearchSuggestionParser::Results unused_results;
    StoreRemoteResponse(SearchSuggestionParser::ExtractJsonData(
                            source, std::move(response_body)),
                        client(), input, result_type,
                        /*is_prefetch=*/true, &unused_results);
  }

  prefetch_loader_.reset();
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
  match.suggest_type = navigation.suggest_type();
  for (const int subtype : navigation.subtypes()) {
    match.subtypes.insert(SuggestSubtypeForNumber(subtype));
  }
  return match;
}

void ZeroSuggestProvider::ConvertSuggestResultsToAutocompleteMatches(
    const SearchSuggestionParser::Results& results,
    const AutocompleteInput& input) {
  matches_.clear();
  suggestion_groups_map_.clear();
  experiment_stats_v2s_.clear();

  if (results.field_trial_triggered) {
    client()->GetOmniboxTriggeredFeatureService()->FeatureTriggered(
        metrics::OmniboxEventProto_Feature_REMOTE_ZERO_SUGGEST_FEATURE);
  }

  // Add all the SuggestResults to the map. We display all ZeroSuggest search
  // suggestions as unbolded.
  MatchMap map;
  for (size_t i = 0; i < results.suggest_results.size(); ++i) {
    AddMatchToMap(results.suggest_results[i], std::string(), input,
                  client()->GetTemplateURLService()->GetDefaultSearchProvider(),
                  client()->GetTemplateURLService()->search_terms_data(), i,
                  false, false, &map);
  }

  const int num_query_results = map.size();
  const int num_nav_results = results.navigation_results.size();
  const int num_results = num_query_results + num_nav_results;
  base::UmaHistogramCounts1M("ZeroSuggest.QueryResults", num_query_results);
  base::UmaHistogramCounts1M("ZeroSuggest.URLResults", num_nav_results);
  base::UmaHistogramCounts1M("ZeroSuggest.AllResults", num_results);

  if (num_results == 0) {
    return;
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
}
