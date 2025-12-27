// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/zero_suggest_provider.h"

#include <stddef.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/char_iterator.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/omnibox/browser/autocomplete_enums.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_debouncer.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/omnibox_triggered_feature_service.h"
#include "components/omnibox/browser/page_classification_functions.h"
#include "components/omnibox/browser/remote_suggestions_service.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/omnibox/browser/suggestion_group_util.h"
#include "components/omnibox/browser/zero_suggest_cache_service.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/template_url_service.h"
#include "components/url_formatter/url_formatter.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "url/gurl.h"
#include "url/url_canon.h"
#include "url/url_util.h"

using OEP = metrics::OmniboxEventProto;
using OFT = metrics::OmniboxFocusType;
using OIT = metrics::OmniboxInputType;

namespace {

// Maximum length of page title sent to Suggest via `pageTitle` CGI param,
// expressed as number of Unicode characters (codepoints).
//
// NOTE: The actual string value for the CGI param could be longer (in bytes)
// than this number, due to the way we're encoding the page title before sending
// it to Suggest. In the worst-case scenario, the total number of bytes sent
// could be up to 12x the value specified here:
// `kMaxPageTitleLength` (# of codepoints) x 4 (UTF-8 code-units per codepoint
// [maximum]) x 3 (due to URL-encoding [worst-case]).
const size_t kMaxPageTitleLength = 128;

using ResultType = ZeroSuggestProvider::ResultType;
constexpr bool is_ios = !!BUILDFLAG(IS_IOS);

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

// Called in Start() or StartPrefetch(), logs eligibility UMA histogram. Must be
// called exactly once. Otherwise the meaning of the the histogram it logs would
// change.
void LogOmniboxZeroSuggestEligibility(ResultType result_type, bool eligible) {
  Eligibility eligibility = Eligibility::kEligible;
  switch (result_type) {
    case ResultType::kRemoteNoURL: {
      if (!eligible) {
        eligibility = Eligibility::kRequestNoURLIneligible;
      }
      break;
    }
    case ResultType::kRemoteSendURL: {
      if (!eligible) {
        eligibility = Eligibility::kRequestSendURLIneligible;
      }
      break;
    }
    case ResultType::kNone: {
      eligibility = Eligibility::kGenerallyIneligible;
      break;
    }
  }
  base::UmaHistogramEnumeration("Omnibox.ZeroSuggestProvider.Eligibility",
                                eligibility);
}

void LogOmniboxZeroSuggestRequest(const RemoteRequestEvent request_event,
                                  const ResultType result_type,
                                  const bool is_prefetch) {
  DCHECK_NE(ResultType::kNone, result_type);

  const std::string result_type_string =
      result_type == ResultType::kRemoteNoURL ? ".NoURL" : ".URLBased";
  const std::string request_type_string =
      is_prefetch ? ".Prefetch" : ".NonPrefetch";
  base::UmaHistogramEnumeration(
      "Omnibox.ZeroSuggestProvider" + result_type_string + request_type_string,
      request_event);
}

// Called in StoreRemoteResponse() and ReadStoredResponse() to determine if the
// zero suggest cache is being used to store ZPS responses received from the
// remote Suggest service for the given |result_type|.
bool ShouldCacheResultTypeInContext(const ResultType result_type,
                                    const bool has_contextual_input,
                                    const OEP::PageClassification page_class) {
  switch (result_type) {
    case ResultType::kRemoteNoURL:
      // Only cache results for the NTP realbox if there is no contextual input.
      if (omnibox::IsNTPRealbox(page_class)) {
        return !has_contextual_input;
      }
      return !(omnibox::IsLensSearchbox(page_class) ||
               omnibox::IsComposebox(page_class));
    case ResultType::kRemoteSendURL:
      return omnibox::IsSearchResultsPage(page_class)
                 ? base::FeatureList::IsEnabled(
                       omnibox::kZeroSuggestPrefetchingOnSRP)
                 : base::FeatureList::IsEnabled(
                       omnibox::kZeroSuggestPrefetchingOnWeb);
    case ResultType::kNone:
      NOTREACHED() << "kNone is not a valid zero suggest result type.";
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
          /*default_result_relevance=*/
          omnibox::kDefaultRemoteZeroSuggestRelevance,
          /*is_keyword_result=*/false, results)) {
    return false;
  }

  const bool has_contextual_input =
      input.lens_overlay_suggest_inputs().has_value();
  const auto page_class = input.current_page_classification();
  if (!ShouldCacheResultTypeInContext(result_type, has_contextual_input,
                                      page_class)) {
    return true;
  }

  // Force use of empty page URL when given an input with "No URL" result type.
  const std::string page_url = result_type != ResultType::kRemoteNoURL
                                   ? input.current_url().spec()
                                   : std::string();
  client->GetZeroSuggestCacheService()->StoreZeroSuggestResponse(page_url,
                                                                 response_json);
  LogOmniboxZeroSuggestRequest(RemoteRequestEvent::kResponseCached, result_type,
                               is_prefetch);
  return true;
}

// Called in Start().
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
  const bool has_contextual_input =
      input.lens_overlay_suggest_inputs().has_value();
  if (!ShouldCacheResultTypeInContext(result_type, has_contextual_input,
                                      page_class)) {
    return false;
  }

  // Force use of empty page URL when given an input with "No URL" result type.
  const std::string page_url = result_type != ResultType::kRemoteNoURL
                                   ? input.current_url().spec()
                                   : std::string();
  std::string response_json = client->GetZeroSuggestCacheService()
                                  ->ReadZeroSuggestResponse(page_url)
                                  .response_json;
  if (response_json.empty()) {
    return false;
  }

  std::optional<base::Value::List> response_data =
      SearchSuggestionParser::DeserializeJsonData(response_json);
  if (!response_data) {
    return false;
  }

  if (!SearchSuggestionParser::ParseSuggestResults(
          *response_data, input, client->GetSchemeClassifier(),
          /*default_result_relevance=*/
          omnibox::kDefaultRemoteZeroSuggestRelevance,
          /*is_keyword_result=*/false, results)) {
    return false;
  }

  return true;
}

// Returns the type of results that should be generated for the given input.
// It does not check whether zero-suggest requests can be made. Those checks
// are done in GetResultTypeAndEligibility().
ResultType ResultTypeForInput(const AutocompleteInput& input) {
  const auto page_class = input.current_page_classification();

  // Disallow non-zero-prefix inputs.
  if (!input.IsZeroSuggest()) {
    return ResultType::kNone;
  }

  // Android Search Widget.
  if (page_class == OEP::ANDROID_SHORTCUTS_WIDGET) {
    return ResultType::kRemoteNoURL;
  }

  // New Tab Page.
  if (omnibox::IsNTPPage(page_class)) {
    if (input.type() == OIT::EMPTY) {
      return ResultType::kRemoteNoURL;
    }
  }

  // Composebox
  if (omnibox::IsComposebox(page_class)) {
    if (input.type() == OIT::EMPTY) {
      return ResultType::kRemoteNoURL;
    }
  }

  // Lens unimodal, multimodal, and contextual searchboxes.
  if (omnibox::IsLensSearchbox(page_class)) {
    if (input.type() == OIT::EMPTY) {
      return ResultType::kRemoteNoURL;
    }
  }

  // The following cases require sending the current page URL in the request.
  // Ensure the page URL is valid with an HTTP(S) scheme and is not the NTP URL.
  if (!BaseSearchProvider::PageURLIsEligibleForSuggestRequest(
          input.current_url(), page_class)) {
    return ResultType::kNone;
  }

  // Open Web and Search Results Page.
  if (omnibox::IsOtherWebPage(page_class) ||
      omnibox::IsSearchResultsPage(page_class)) {
    if (input.type() == OIT::URL &&
        (is_ios || base::FeatureList::IsEnabled(
                       omnibox::kFocusTriggersWebAndSRPZeroSuggest))) {
      return ResultType::kRemoteSendURL;
    }
    if (input.type() == OIT::EMPTY && !is_ios) {
      return ResultType::kRemoteSendURL;
    }
  }

  return ResultType::kNone;
}

std::u16string TruncateUTF16(const std::u16string& input, size_t max_length) {
  if (input.empty()) {
    return u"";
  }

  size_t num_chars = 0;
  base::i18n::UTF16CharIterator it(input);
  while (!it.end() && (num_chars < max_length)) {
    it.Advance();
    num_chars++;
  }

  return input.substr(0, it.array_pos());
}

std::string EncodeURIComponent(const std::string& component) {
  url::RawCanonOutputT<char> encoded;
  url::EncodeURIComponent(component, &encoded);
  return std::string(encoded.view());
}

void MaybeAddContextualSuggestParams(
    const AutocompleteProviderClient* client,
    const AutocompleteInput& input,
    TemplateURLRef::SearchTermsArgs& search_terms_args) {
  // Do not add the contextual suggest params if Lens is not enabled to fulfill
  // the suggestion, unless the input contains context data, in which case lens
  // will not be used to fulfill the suggestion.
  if (!client->IsLensEnabled() && input.context_tab_title().empty()) {
    return;
  }

  std::vector<std::string> additional_query_params;

  if ((!search_terms_args.current_page_url.empty() &&
       omnibox::IsOtherWebPage(search_terms_args.page_classification)) ||
      !input.context_tab_title().empty()) {
    // Add "ctxus=" CGI param.
    std::string_view contextual_url_suggest_param =
        omnibox_feature_configs::ContextualSearch::Get()
            .contextual_url_suggest_param;
    if (client->ShouldSendContextualUrlSuggestParam() &&
        !contextual_url_suggest_param.empty()) {
      additional_query_params.push_back(
          base::StrCat({"ctxus=", contextual_url_suggest_param}));
    }

    // Add "pageTitle=" CGI param.
    if (omnibox_feature_configs::ContextualSearch::Get()
            .send_page_title_suggest_param &&
        client->IsPersonalizedUrlDataCollectionActive()) {
      std::string page_title =
          !input.context_tab_title().empty()
              ? EncodeURIComponent(base::UTF16ToUTF8(TruncateUTF16(
                    input.context_tab_title(), kMaxPageTitleLength)))
              : EncodeURIComponent(base::UTF16ToUTF8(
                    TruncateUTF16(input.current_title(), kMaxPageTitleLength)));
      if (client->ShouldSendPageTitleSuggestParam() && !page_title.empty()) {
        additional_query_params.push_back(
            base::StrCat({"pageTitle=", page_title}));
      }
      if (!input.context_tab_url().is_empty()) {
        search_terms_args.current_page_url = input.context_tab_url().spec();
      }
    } else if (search_terms_args.page_classification ==
                   metrics::OmniboxEventProto::NTP_COMPOSEBOX &&
               !client->IsPersonalizedUrlDataCollectionActive() &&
               !input.context_tab_title().empty()) {
      // Set `lens_overlay_suggest_inputs` when history sync is disabled, but
      // delayed context data is found in the input. This is done to switch over
      // to the "chrome-contextual" client to only retrieve contextual
      // suggestions.
      search_terms_args.lens_overlay_suggest_inputs =
          lens::proto::LensOverlaySuggestInputs();
    }

    search_terms_args.additional_query_params =
        base::JoinString(additional_query_params, "&");
  }
}

}  // namespace

// static
ZeroSuggestProvider* ZeroSuggestProvider::Create(
    AutocompleteProviderClient* client,
    AutocompleteProviderListener* listener) {
  return new ZeroSuggestProvider(client, listener);
}

// static
AutocompleteMatch ZeroSuggestProvider::NavigationToMatch(
    AutocompleteProvider* provider,
    AutocompleteProviderClient* client,
    const SearchSuggestionParser::NavigationResult& navigation) {
  AutocompleteMatch match(provider, navigation.relevance(), false,
                          navigation.type());
  match.destination_url = navigation.url();

  match.fill_into_edit +=
      AutocompleteInput::FormattedStringWithEquivalentMeaning(
          navigation.url(), url_formatter::FormatUrl(navigation.url()),
          client->GetSchemeClassifier(), nullptr);

  // Zero suggest results should always omit protocols and never appear bold.
  auto format_types = AutocompleteMatch::GetFormatTypes(false, false);
  match.contents = url_formatter::FormatUrl(navigation.url(), format_types,
                                            base::UnescapeRule::SPACES, nullptr,
                                            nullptr, nullptr);
  match.contents_class = ClassifyTermMatches({}, match.contents.length(),
                                             ACMatchClassification::NONE,
                                             ACMatchClassification::URL);

  match.description =
      AutocompleteMatch::SanitizeString(navigation.description());
  match.description_class = ClassifyTermMatches({}, match.description.length(),
                                                ACMatchClassification::NONE,
                                                ACMatchClassification::NONE);
  match.suggest_type = navigation.suggest_type();
  for (const int subtype : navigation.subtypes()) {
    match.subtypes.insert(SuggestSubtypeForNumber(subtype));
  }
  return match;
}

// static
void ZeroSuggestProvider::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(omnibox::kZeroSuggestCachedResults,
                               std::string());
  registry->RegisterDictionaryPref(omnibox::kZeroSuggestCachedResultsWithURL);
}

// static
std::pair<ZeroSuggestProvider::ResultType, bool>
ZeroSuggestProvider::GetResultTypeAndEligibility(
    const AutocompleteProviderClient* client,
    const AutocompleteInput& input) {
  const auto result_type = ResultTypeForInput(input);

  const auto* template_url_service = client->GetTemplateURLService();
  if (!template_url_service) {
    return std::make_pair(result_type, /*eligibility=*/false);
  }

  auto eligibility = true;
  switch (result_type) {
    case ResultType::kRemoteNoURL: {
      if (!CanSendSecureSuggestRequest(
              input.current_page_classification(),
              template_url_service->GetDefaultSearchProvider(),
              template_url_service->search_terms_data(), client)) {
        eligibility = false;
      }
      break;
    }
    case ResultType::kRemoteSendURL: {
      if (!CanSendSuggestRequestWithPageURL(
              input.current_url(), input.current_page_classification(),
              template_url_service->GetDefaultSearchProvider(),
              template_url_service->search_terms_data(), client)) {
        eligibility = false;
      }
      break;
    }
    case ResultType::kNone: {
      eligibility = false;
      break;
    }
  }
  return std::make_pair(result_type, eligibility);
}

void ZeroSuggestProvider::StartPrefetch(const AutocompleteInput& input) {
  AutocompleteProvider::StartPrefetch(input);

  TRACE_EVENT0("omnibox", "ZeroSuggestProvider::StartPrefetch");

  if (!OmniboxFieldTrial::IsZeroSuggestPrefetchingEnabledInContext(
          input.current_page_classification()) &&
      !omnibox_feature_configs::ContextualSearch::Get()
           .IsEnabledWithPrefetch()) {
    return;
  }

  auto [result_type, eligible] = GetResultTypeAndEligibility(client(), input);
  LogOmniboxZeroSuggestEligibility(result_type, eligible);
  if (!eligible) {
    return;
  }

  if (base::FeatureList::IsEnabled(omnibox::kZeroSuggestPrefetchDebouncing)) {
    debouncer_->RequestRun(
        base::BindOnce(&ZeroSuggestProvider::RunZeroSuggestPrefetch,
                       base::Unretained(this), input, result_type));
  } else {
    RunZeroSuggestPrefetch(input, result_type);
  }
}

void ZeroSuggestProvider::RunZeroSuggestPrefetch(const AutocompleteInput& input,
                                                 const ResultType result_type) {
  TemplateURLRef::SearchTermsArgs search_terms_args;
  search_terms_args.page_classification = input.current_page_classification();
  search_terms_args.request_source = input.request_source();
  search_terms_args.focus_type = input.focus_type();
  search_terms_args.current_page_url = result_type == ResultType::kRemoteSendURL
                                           ? input.current_url().spec()
                                           : std::string();
  search_terms_args.lens_overlay_suggest_inputs =
      input.lens_overlay_suggest_inputs();

  MaybeAddContextualSuggestParams(client(), input, search_terms_args);

  std::unique_ptr<network::SimpleURLLoader>* prefetch_loader = nullptr;
  if (result_type == ResultType::kRemoteNoURL) {
    prefetch_loader = &ntp_prefetch_loader_;
  } else if (result_type == ResultType::kRemoteSendURL) {
    prefetch_loader = &srp_web_prefetch_loader_;
  } else {
    NOTREACHED();
  }

  // If the app is currently in the background state, do not initiate ZPS
  // prefetch requests. This helps to conserve CPU cycles on iOS while
  // in the background state.
  if (client()->in_background_state()) {
    return;
  }

  if (*prefetch_loader) {
    LogOmniboxZeroSuggestRequest(RemoteRequestEvent::kRequestInvalidated,
                                 result_type,
                                 /*is_prefetch=*/true);
  }

  const auto* template_url_service = client()->GetTemplateURLService();
  // Create a loader for the appropriate page context and take ownership of it.
  // Prefetching zero-prefix suggestions in OTR contexts is not allowed.
  DCHECK(!client()->IsOffTheRecord());
  *prefetch_loader =
      client()
          ->GetRemoteSuggestionsService(/*create_if_necessary=*/true)
          ->StartZeroPrefixSuggestionsRequest(
              RemoteRequestType::kZeroSuggestPrefetch,
              client()->IsOffTheRecord(),
              template_url_service->GetDefaultSearchProvider(),
              search_terms_args, template_url_service->search_terms_data(),
              base::BindOnce(&ZeroSuggestProvider::OnPrefetchURLLoadComplete,
                             weak_ptr_factory_.GetWeakPtr(), input,
                             result_type));

  LogOmniboxZeroSuggestRequest(RemoteRequestEvent::kRequestSent, result_type,
                               /*is_prefetch=*/true);
}

void ZeroSuggestProvider::Start(const AutocompleteInput& input,
                                bool minimal_changes) {
  TRACE_EVENT0("omnibox", "ZeroSuggestProvider::Start");
  Stop(AutocompleteStopReason::kClobbered);

  auto [result_type, eligible] = GetResultTypeAndEligibility(client(), input);
  LogOmniboxZeroSuggestEligibility(result_type, eligible);
  if (!eligible) {
    return;
  }

  result_type_running_ = result_type;

  // Convert the stored response to |matches_|, if applicable.
  SearchSuggestionParser::Results results;
  if (ReadStoredResponse(client(), input, result_type_running_, &results)) {
    ConvertSuggestResultsToAutocompleteMatches(results, input);
    LogOmniboxZeroSuggestRequest(
        RemoteRequestEvent::kCachedResponseConvertedToMatches,
        result_type_running_,
        /*is_prefetch=*/false);
  }

  // Do not start a request if async requests are disallowed.
  if (input.omit_asynchronous_matches()) {
    return;
  }

  done_ = false;

  TemplateURLRef::SearchTermsArgs search_terms_args;
  search_terms_args.page_classification = input.current_page_classification();
  search_terms_args.request_source = input.request_source();
  search_terms_args.focus_type = input.focus_type();
  search_terms_args.current_page_url =
      result_type_running_ == ResultType::kRemoteSendURL
          ? input.current_url().spec()
          : std::string();
  search_terms_args.lens_overlay_suggest_inputs =
      input.lens_overlay_suggest_inputs();
  search_terms_args.aim_tool_mode = input.aim_tool_mode();

  MaybeAddContextualSuggestParams(client(), input, search_terms_args);

  const auto* template_url_service = client()->GetTemplateURLService();
  // Create a loader for the request and take ownership of it.
  // Request for zero-prefix suggestions in OTR contexts is not allowed; except
  // for the Lens searchboxes.
  DCHECK(!client()->IsOffTheRecord() ||
         omnibox::IsLensSearchbox(input.current_page_classification()));
  loader_ =
      client()
          ->GetRemoteSuggestionsService(/*create_if_necessary=*/true)
          ->StartZeroPrefixSuggestionsRequest(
              RemoteRequestType::kZeroSuggest, client()->IsOffTheRecord(),
              template_url_service->GetDefaultSearchProvider(),
              search_terms_args, template_url_service->search_terms_data(),
              base::BindOnce(&ZeroSuggestProvider::OnURLLoadComplete,
                             weak_ptr_factory_.GetWeakPtr(), input,
                             result_type_running_));

  LogOmniboxZeroSuggestRequest(RemoteRequestEvent::kRequestSent,
                               result_type_running_,
                               /*is_prefetch=*/false);
}

void ZeroSuggestProvider::Stop(AutocompleteStopReason stop_reason) {
  AutocompleteProvider::Stop(stop_reason);

  if (base::FeatureList::IsEnabled(omnibox::kZeroSuggestPrefetchDebouncing)) {
    debouncer_->CancelRequest();
  }

  if (loader_) {
    LogOmniboxZeroSuggestRequest(RemoteRequestEvent::kRequestInvalidated,
                                 result_type_running_,
                                 /*is_prefetch=*/false);
    loader_.reset();
  }
  result_type_running_ = ResultType::kNone;

  if (stop_reason == AutocompleteStopReason::kClobbered) {
    experiment_stats_v2s_.clear();
    gws_event_id_hashes_.clear();
  }
}

void ZeroSuggestProvider::DeleteMatch(const AutocompleteMatch& match) {
  // Remove the deleted match from the cache, so it is not shown to the user
  // again. Since the deleted result might have been surfaced as a suggestion on
  // both NTP and SRP/Web, blow away the entire cache.
  client()->GetZeroSuggestCacheService()->ClearCache();

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

  if (base::FeatureList::IsEnabled(omnibox::kZeroSuggestPrefetchDebouncing)) {
    debouncer_ = std::make_unique<AutocompleteProviderDebouncer>(
        OmniboxFieldTrial::kZeroSuggestPrefetchDebounceFromLastRun.Get(),
        OmniboxFieldTrial::kZeroSuggestPrefetchDebounceDelay.Get());
  }
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
    const int response_code,
    std::optional<std::string> response_body) {
  TRACE_EVENT0("omnibox", "ZeroSuggestProvider::OnURLLoadComplete");

  DCHECK(!done_);
  DCHECK_EQ(loader_.get(), source);

  if (response_code != 200) {
    loader_.reset();
    done_ = true;
    return;
  }

  LogOmniboxZeroSuggestRequest(RemoteRequestEvent::kResponseReceived,
                               result_type,
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

  // The Contextual Search in Omnibox experience, which is only active on Web
  // page context, intentionally updates the cache with latest received results,
  // but does not publish the matches asynchronously.
  if (input.current_page_classification() ==
          metrics::OmniboxEventProto::OTHER &&
      omnibox_feature_configs::ContextualSearch::Get()
          .IsEnabledWithPrefetch()) {
    return;
  }

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
  LogOmniboxZeroSuggestRequest(RemoteRequestEvent::kResponseConvertedToMatches,
                               result_type,
                               /*is_prefetch=*/false);
  NotifyListeners(/*updated_matches=*/true);
}

void ZeroSuggestProvider::OnPrefetchURLLoadComplete(
    const AutocompleteInput& input,
    const ResultType result_type,
    const network::SimpleURLLoader* source,
    const int response_code,
    std::optional<std::string> response_body) {
  TRACE_EVENT0("omnibox", "ZeroSuggestProvider::OnPrefetchURLLoadComplete");

  std::unique_ptr<network::SimpleURLLoader>* prefetch_loader = nullptr;
  if (result_type == ResultType::kRemoteNoURL) {
    prefetch_loader = &ntp_prefetch_loader_;
  } else if (result_type == ResultType::kRemoteSendURL) {
    prefetch_loader = &srp_web_prefetch_loader_;
  } else {
    NOTREACHED();
  }

  DCHECK_EQ(prefetch_loader->get(), source);

  if (response_code == 200) {
    LogOmniboxZeroSuggestRequest(RemoteRequestEvent::kResponseReceived,
                                 result_type,
                                 /*is_prefetch=*/true);

    // If the app is currently in the background state, do not parse and store
    // ZPS prefetch responses. This helps to conserve CPU cycles on iOS while in
    // the background state.
    // If `kZeroSuggestPrefetchingOnSRPCounterfactual` has been enabled, we also
    // ignore any ZPS prefetch response on SRP, ensuring that ZPS prefetching
    // is essentially a no-op in this case.
    if (!client()->in_background_state() &&
        !(OmniboxFieldTrial::kZeroSuggestPrefetchingOnSRPCounterfactual.Get() &&
          input.current_page_classification() ==
              metrics::OmniboxEventProto::SRP_ZPS_PREFETCH &&
          result_type == ResultType::kRemoteSendURL)) {
      SearchSuggestionParser::Results unused_results;
      StoreRemoteResponse(SearchSuggestionParser::ExtractJsonData(
                              source, std::move(response_body)),
                          client(), input, result_type,
                          /*is_prefetch=*/true, &unused_results);
    }
  }

  prefetch_loader->reset();
}

void ZeroSuggestProvider::ConvertSuggestResultsToAutocompleteMatches(
    const SearchSuggestionParser::Results& results,
    const AutocompleteInput& input) {
  matches_.clear();
  suggestion_groups_map_.clear();
  experiment_stats_v2s_.clear();
  gws_event_id_hashes_.clear();

  if (results.field_trial_triggered) {
    client()->GetOmniboxTriggeredFeatureService()->FeatureTriggered(
        metrics::OmniboxEventProto_Feature_REMOTE_ZERO_SUGGEST_FEATURE);
  }

  // Add all the SuggestResults to the map. We display all ZeroSuggest search
  // suggestions as unbolded.
  MatchMap map;
  for (size_t i = 0; i < results.suggest_results.size(); ++i) {
    AddMatchToMap(results.suggest_results[i], input,
                  client()->GetTemplateURLService()->GetDefaultSearchProvider(),
                  client()->GetTemplateURLService()->search_terms_data(), i,
                  false, false, &map);
  }

  const int num_query_results = map.size();
  const int num_nav_results = results.navigation_results.size();
  const int num_results = num_query_results + num_nav_results;
  base::UmaHistogramCounts1M("Omnibox.ZeroSuggest.QueryResults",
                             num_query_results);
  base::UmaHistogramCounts1M("Omnibox.ZeroSuggest.URLResults", num_nav_results);
  base::UmaHistogramCounts1M("Omnibox.ZeroSuggest.AllResults", num_results);

  if (num_results == 0) {
    return;
  }

  for (MatchMap::const_iterator it(map.begin()); it != map.end(); ++it) {
    matches_.push_back(it->second);
  }

  const SearchSuggestionParser::NavigationResults& nav_results(
      results.navigation_results);
  for (const auto& nav_result : nav_results) {
    matches_.push_back(NavigationToMatch(this, client(), nav_result));
  }

  // Update the suggestion groups information from the server response.
  for (const auto& entry : results.suggestion_groups_map) {
    suggestion_groups_map_[entry.first].MergeFrom(entry.second);
  }

  // Update the list of experiment stats from the server response.
  for (const auto& experiment_stats_v2 : results.experiment_stats_v2s) {
    experiment_stats_v2s_.push_back(experiment_stats_v2);
  }

  // Update the list of GWS event ID hashes from the server response.
  for (const auto& gws_event_id_hash : results.gws_event_id_hashes) {
    gws_event_id_hashes_.push_back(gws_event_id_hash);
  }
}
