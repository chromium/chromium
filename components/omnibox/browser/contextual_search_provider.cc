// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/contextual_search_provider.h"

#include <stddef.h>

#include <cmath>
#include <optional>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/omnibox/browser/actions/contextual_search_action.h"
#include "components/omnibox/browser/autocomplete_enums.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_debouncer.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/lens_suggest_inputs_utils.h"
#include "components/omnibox/browser/match_compare.h"
#include "components/omnibox/browser/page_classification_functions.h"
#include "components/omnibox/browser/remote_suggestions_service.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/omnibox/browser/suggestion_group_util.h"
#include "components/omnibox/browser/zero_suggest_provider.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/template_url_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

// The internal default verbatim match relevance.
constexpr int kDefaultVerbatimMatchRelevance = 1500;

// Populates |results| with the response if it can be successfully parsed for
// |input|. Returns true if the response can be successfully parsed.
bool ParseRemoteResponse(const std::string& response_json,
                         AutocompleteProviderClient* client,
                         const AutocompleteInput& input,
                         SearchSuggestionParser::Results* results) {
  DCHECK(results);
  if (response_json.empty()) {
    return false;
  }

  std::optional<base::Value::List> response_data =
      SearchSuggestionParser::DeserializeJsonData(response_json);
  if (!response_data) {
    return false;
  }

  return SearchSuggestionParser::ParseSuggestResults(
      *response_data, input, client->GetSchemeClassifier(),
      /*default_result_relevance=*/omnibox::kDefaultRemoteZeroSuggestRelevance,
      /*is_keyword_result=*/true, results);
}

}  // namespace

void ContextualSearchProvider::Start(
    const AutocompleteInput& autocomplete_input,
    bool minimal_changes) {
  TRACE_EVENT0("omnibox", "ContextualSearchProvider::Start");
  // Clear the cached results to remove the page search action matches. Also,
  // matches the behavior of the `ZeroSuggestProvider`.
  Stop(AutocompleteStopReason::kClobbered);

  if (client()->IsOffTheRecord()) {
    done_ = true;
    return;
  }

  const auto [input, starter_pack_engine] = AdjustInputForStarterPackKeyword(
      autocomplete_input, client()->GetTemplateURLService());
  if (!starter_pack_engine) {
    // Only surface the action match that helps the user find their way to Lens.
    // Requirements: web or SRP, non-NTP, with empty input, and local files are
    // allowed but not other local schemes.
    if ((omnibox::IsOtherWebPage(input.current_page_classification()) ||
         omnibox::IsSearchResultsPage(input.current_page_classification())) &&
        (input.current_url().SchemeIsHTTPOrHTTPS() ||
         input.current_url().SchemeIs(url::kFileScheme)) &&
        input.IsZeroSuggest() && client()->IsLensEnabled()) {
      AddPageSearchActionMatches(input);
    }
    return;
  }
  input_keyword_ = starter_pack_engine->keyword();

  AddDefaultVerbatimMatch(input);

  // Exit early if the input is not in ZPS keyword mode or the autocomplete
  // input is not allowed to make asynchronous requests.
  if (!input.text().empty() || autocomplete_input.omit_asynchronous_matches()) {
    done_ = true;
    return;
  }
  done_ = false;
  StartSuggestRequest(std::move(input));
}

void ContextualSearchProvider::Stop(AutocompleteStopReason stop_reason) {
  // If the stop is due to user inactivity, the request will continue so the
  // suggestions can be shown when they are ready.
  if (stop_reason == AutocompleteStopReason::kInactivity) {
    return;
  }
  AutocompleteProvider::Stop(stop_reason);
  lens_suggest_inputs_subscription_ = {};
  loader_.reset();
  input_keyword_.clear();
}

void ContextualSearchProvider::AddProviderInfo(
    ProvidersInfo* provider_info) const {
  BaseSearchProvider::AddProviderInfo(provider_info);
  if (!matches().empty()) {
    provider_info->back().set_times_returned_results_in_session(1);
  }
}

ContextualSearchProvider::ContextualSearchProvider(
    AutocompleteProviderClient* client,
    AutocompleteProviderListener* listener)
    : BaseSearchProvider(AutocompleteProvider::TYPE_CONTEXTUAL_SEARCH, client) {
  AddListener(listener);
}

ContextualSearchProvider::~ContextualSearchProvider() = default;

bool ContextualSearchProvider::ShouldAppendExtraParams(
    const SearchSuggestionParser::SuggestResult& result) const {
  // We always use the default provider for search, so append the params.
  return true;
}

void ContextualSearchProvider::StartSuggestRequest(AutocompleteInput input) {
  if (AreLensSuggestInputsReady(input.lens_overlay_suggest_inputs()) ||
      !omnibox_feature_configs::ContextualSearch::Get()
           .csp_async_suggest_inputs) {
    // If the suggest inputs are ready, make the suggest request immediately.
    // Also, skip the async wait if the feature is disabled.
    MakeSuggestRequest(std::move(input));
    return;
  }

  // Wait for the suggest inputs to be generated and then make the suggest
  // request. Safe to use base::Unretained(this) because the subscription is
  // reset and cancelled if this provider is destroyed.
  lens_suggest_inputs_subscription_ = client()->GetLensSuggestInputsWhenReady(
      base::BindOnce(&ContextualSearchProvider::OnLensSuggestInputsReady,
                     base::Unretained(this), std::move(input)));
}

void ContextualSearchProvider::OnLensSuggestInputsReady(
    AutocompleteInput input,
    std::optional<lens::proto::LensOverlaySuggestInputs> lens_suggest_inputs) {
  CHECK(!AreLensSuggestInputsReady(input.lens_overlay_suggest_inputs()));
  if (lens_suggest_inputs) {
    input.set_lens_overlay_suggest_inputs(*lens_suggest_inputs);
  }
  MakeSuggestRequest(std::move(input));
}

void ContextualSearchProvider::MakeSuggestRequest(AutocompleteInput input) {
  TemplateURLRef::SearchTermsArgs search_terms_args;

  // TODO(crbug.com/404608703): Consider new types or taking from `input`.
  search_terms_args.page_classification =
      metrics::OmniboxEventProto::CONTEXTUAL_SEARCHBOX;
  search_terms_args.request_source =
      SearchTermsData::RequestSource::LENS_OVERLAY;

  search_terms_args.focus_type = input.focus_type();
  search_terms_args.current_page_url = input.current_url().spec();
  search_terms_args.lens_overlay_suggest_inputs =
      input.lens_overlay_suggest_inputs();

  // Make the request and store the loader to keep it alive. Destroying the
  // loader will cancel the request. Safe to use base::Unretained(this) because
  // the loader is reset and destroyed if this provider is destroyed.
  loader_ =
      client()
          ->GetRemoteSuggestionsService(/*create_if_necessary=*/true)
          ->StartZeroPrefixSuggestionsRequest(
              RemoteRequestType::kZeroSuggest, client()->IsOffTheRecord(),
              client()->GetTemplateURLService()->GetDefaultSearchProvider(),
              search_terms_args,
              client()->GetTemplateURLService()->search_terms_data(),
              base::BindOnce(&ContextualSearchProvider::SuggestRequestCompleted,
                             base::Unretained(this), std::move(input)));
}

void ContextualSearchProvider::SuggestRequestCompleted(
    AutocompleteInput input,
    const network::SimpleURLLoader* source,
    const int response_code,
    std::unique_ptr<std::string> response_body) {
  DCHECK(!done_);
  DCHECK_EQ(loader_.get(), source);

  if (response_code != 200) {
    loader_.reset();
    done_ = true;
    return;
  }

  // Some match must be available in order to stay in keyword mode,
  // but an empty result set is possible. The default match will
  // always be added first for a consistent keyword experience.
  matches_.clear();
  suggestion_groups_map_.clear();
  AddDefaultVerbatimMatch(input);

  // Note: Queries are not yet supported. If it is kept, the current behavior
  // will be to mismatch between input text and query empty string, failing
  // the parse early in SearchSuggestionParser::ParseSuggestResults.
  input.UpdateText(u"", 0u, {});

  SearchSuggestionParser::Results results;
  if (!ParseRemoteResponse(SearchSuggestionParser::ExtractJsonData(
                               source, std::move(response_body)),
                           client(), input, &results)) {
    loader_.reset();
    done_ = true;
    return;
  }

  loader_.reset();
  done_ = true;

  // Convert the results into |matches_| and notify the listeners.
  ConvertSuggestResultsToAutocompleteMatches(results, input);
  NotifyListeners(/*updated_matches=*/true);
}

void ContextualSearchProvider::ConvertSuggestResultsToAutocompleteMatches(
    const SearchSuggestionParser::Results& results,
    const AutocompleteInput& input) {
  const TemplateURL* template_url = GetKeywordTemplateURL();
  if (!template_url) {
    return;
  }
  // Add all the SuggestResults to the map. We display all ZeroSuggest search
  // suggestions as unbolded.
  MatchMap map;
  for (size_t i = 0; i < results.suggest_results.size(); ++i) {
    AddMatchToMap(results.suggest_results[i], input, template_url,
                  client()->GetTemplateURLService()->search_terms_data(), i,
                  /*mark_as_deletable*/ false, /*in_keyword_mode=*/true, &map);
  }

  const int num_query_results = map.size();
  const int num_nav_results = results.navigation_results.size();
  const int num_results = num_query_results + num_nav_results;

  if (num_results == 0) {
    return;
  }

  for (MatchMap::const_iterator it(map.begin()); it != map.end(); ++it) {
    matches_.push_back(it->second);
  }

  const SearchSuggestionParser::NavigationResults& nav_results(
      results.navigation_results);
  for (const auto& nav_result : nav_results) {
    matches_.push_back(
        ZeroSuggestProvider::NavigationToMatch(this, client(), nav_result));
  }

  // Update the suggestion groups information from the server response.
  for (const auto& entry : results.suggestion_groups_map) {
    suggestion_groups_map_[entry.first].MergeFrom(entry.second);
  }
}

void ContextualSearchProvider::AddPageSearchActionMatches(
    const AutocompleteInput& input) {
  // These matches are effectively pedals that don't require any query matching.
  // Relevance depends on the page class, and selecting an appropriate score is
  // necessary to avoid downstream conflicts in grouping framework sort order.
  AutocompleteMatch match(
      this,
      omnibox::IsSearchResultsPage(input.current_page_classification())
          ? omnibox::kContextualActionZeroSuggestRelevanceLow
          : omnibox::kContextualActionZeroSuggestRelevance,
      false, AutocompleteMatchType::PEDAL);
  match.transition = ui::PAGE_TRANSITION_GENERATED;
  match.suggest_type = omnibox::SuggestType::TYPE_NATIVE_CHROME;
  match.suggestion_group_id = omnibox::GroupId::GROUP_CONTEXTUAL_SEARCH_ACTION;

  // Lens invocation action with secondary text that shows URL host.
  match.takeover_action =
      base::MakeRefCounted<ContextualSearchOpenLensAction>();
  match.contents =
      base::UTF8ToUTF16(url_formatter::StripWWW(input.current_url().host()));
  if (!match.contents.empty()) {
    match.contents_class = {{0, ACMatchClassification::DIM}};
  }
  match.description = match.takeover_action->GetLabelStrings().hint;
  if (!match.description.empty()) {
    match.description_class = {{0, ACMatchClassification::NONE}};
  }
  match.fill_into_edit = match.description;
  matches_.push_back(match);
}

void ContextualSearchProvider::AddDefaultVerbatimMatch(
    const AutocompleteInput& input) {
  const TemplateURL* template_url = GetKeywordTemplateURL();
  std::u16string text = base::CollapseWhitespace(input.text(), false);

  AutocompleteMatch match(this, kDefaultVerbatimMatchRelevance, false,
                          AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED);
  if (text.empty()) {
    // Inert/static keyword mode helper text match for empty input. This match
    // doesn't commit the omnibox when selected, it just stands in to inform the
    // user about how to use the keyword scope they've entered.
    match.contents =
        l10n_util::GetStringUTF16(IDS_STARTER_PACK_PAGE_EMPTY_QUERY_MATCH_TEXT);
    match.contents_class = {{0, ACMatchClassification::DIM}};

    // These are necessary to avoid the omnibox dropping out of keyword mode.
    if (template_url) {
      match.keyword = template_url->keyword();
    }
    match.transition = ui::PAGE_TRANSITION_KEYWORD;
    match.allowed_to_be_default_match = true;
  } else {
    // Verbatim search suggestion, using the keyword `template_url` (@page)
    // instead of default search engine, for a more consistent keyword UX.
    // Note, the SUBTYPE_CONTEXTUAL_SEARCH subtype will cause this match
    // to be fulfilled via ContextualSearchFulfillmentAction `takeover_action`.
    SearchSuggestionParser::SuggestResult verbatim(
        /*suggestion=*/text,
        /*type=*/AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
        /*suggest_type=*/omnibox::TYPE_NATIVE_CHROME,
        /*subtypes=*/{omnibox::SUBTYPE_CONTEXTUAL_SEARCH},
        /*from_keyword=*/true,
        /*navigational_intent=*/omnibox::NAV_INTENT_NONE,
        /*relevance=*/kDefaultVerbatimMatchRelevance,
        /*relevance_from_server=*/false,
        /*input_text=*/text);
    match = CreateSearchSuggestion(
        this, input, /*in_keyword_mode=*/true, verbatim, template_url,
        client()->GetTemplateURLService()->search_terms_data(),
        /*accepted_suggestion=*/0, ShouldAppendExtraParams(verbatim));
  }
  matches_.push_back(match);
}

const TemplateURL* ContextualSearchProvider::GetKeywordTemplateURL() const {
  return client()->GetTemplateURLService()->GetTemplateURLForKeyword(
      input_keyword_);
}
