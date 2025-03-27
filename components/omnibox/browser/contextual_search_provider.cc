// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/contextual_search_provider.h"

#include <stddef.h>

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
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/omnibox/browser/actions/contextual_search_action.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_debouncer.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/match_compare.h"
#include "components/omnibox/browser/page_classification_functions.h"
#include "components/omnibox/browser/remote_suggestions_service.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/omnibox/browser/zero_suggest_provider.h"
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

// Relevance for pedal-like action matches to be provided when not in keyword
// mode and input is empty.
constexpr int kAdvertActionRelevance = 10000;

// The internal default verbatim match relevance.
constexpr int kDefaultMatchRelevance = 1500;

// Relevance value to use if it was not set explicitly by the server.
constexpr int kDefaultSuggestResultRelevance = 100;

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
      /*default_result_relevance=*/kDefaultSuggestResultRelevance,
      /*is_keyword_result=*/true, results);
}

}  // namespace

void ContextualSearchProvider::Start(
    const AutocompleteInput& autocomplete_input,
    bool minimal_changes) {
  TRACE_EVENT0("omnibox", "ContextualSearchProvider::Start");
  Stop(true, false);

  if (client()->IsOffTheRecord()) {
    done_ = true;
    return;
  }

  const auto [input, starter_pack_engine] = AdjustInputForStarterPackKeyword(
      autocomplete_input, client()->GetTemplateURLService());

  if (!starter_pack_engine) {
    // Only surface the action matches that help the user find their way into
    // the '@page' scope. Requirements: non-SRP, non-NTP, with empty input.
    // TODO(crbug.com/406276335): Move and condition on zero suggest response to
    //  the ZeroSuggestProvider so it can inhibit the ad actions for some pages.
    if (omnibox::IsOtherWebPage(input.current_page_classification()) &&
        (input.IsZeroSuggest() ||
         input.type() == metrics::OmniboxInputType::EMPTY)) {
      AddPageSearchActionMatches();
    }
    return;
  }
  input_keyword_ = starter_pack_engine->keyword();

  AddDefaultMatch(input.text());

  if (input.lens_overlay_suggest_inputs().has_value()) {
    done_ = false;
    StartSuggestRequest(std::move(input));
  } else {
    done_ = true;
  }
}

void ContextualSearchProvider::Stop(bool clear_cached_results,
                                    bool due_to_user_inactivity) {
  AutocompleteProvider::Stop(clear_cached_results, due_to_user_inactivity);
  input_keyword_.clear();
  if (loader_) {
    loader_.reset();
  }
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

  loader_ =
      client()
          ->GetRemoteSuggestionsService(/*create_if_necessary=*/true)
          ->StartZeroPrefixSuggestionsRequest(
              RemoteRequestType::kZeroSuggest, client()->IsOffTheRecord(),
              client()->GetTemplateURLService()->GetDefaultSearchProvider(),
              search_terms_args,
              client()->GetTemplateURLService()->search_terms_data(),
              base::BindOnce(&ContextualSearchProvider::SuggestRequestCompleted,
                             weak_ptr_factory_.GetWeakPtr(), std::move(input)));
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

  // Note: Queries are not yet supported. If it is kept, the current behavior
  // will be to mismatch between `input_text` and `query` empty string, failing
  // the parse early in SearchSuggestionParser::ParseSuggestResults.
  std::u16string input_text = input.text();
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
  // Some match must be available in order to stay in keyword mode,
  // but an empty result set is possible. The default match will
  // always be added first for a consistent keyword experience.
  matches_.clear();
  suggestion_groups_map_.clear();
  AddDefaultMatch(input_text);
  ConvertSuggestResultsToAutocompleteMatches(results, input);
  NotifyListeners(/*updated_matches=*/true);
}

void ContextualSearchProvider::ConvertSuggestResultsToAutocompleteMatches(
    const SearchSuggestionParser::Results& results,
    const AutocompleteInput& input) {
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

void ContextualSearchProvider::AddPageSearchActionMatches() {
  // These matches are effectively pedals that don't require any query matching.
  AutocompleteMatch match(this, kAdvertActionRelevance, false,
                          AutocompleteMatchType::PEDAL);
  match.contents_class = {{0, ACMatchClassification::NONE}};
  match.transition = ui::PAGE_TRANSITION_GENERATED;
  match.suggest_type = omnibox::SuggestType::TYPE_NATIVE_CHROME;

  match.takeover_action =
      base::MakeRefCounted<ContextualSearchAskAboutPageAction>();
  // TODO(crbug.com/399951524): Use action's label strings hint.
  match.contents = u"Ask about this page";
  matches_.push_back(match);

  match.relevance--;
  match.takeover_action =
      base::MakeRefCounted<ContextualSearchSelectRegionAction>();
  // TODO(crbug.com/399951524): Use action's label strings hint.
  match.contents = u"Search with Google Lens";
  matches_.push_back(match);
}

void ContextualSearchProvider::AddDefaultMatch(std::u16string_view input_text) {
  std::u16string_view text =
      base::TrimWhitespace(input_text, base::TrimPositions::TRIM_ALL);

  AutocompleteMatch match(this, kDefaultMatchRelevance, false,
                          AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED);
  if (text.empty()) {
    match.contents =
        l10n_util::GetStringUTF16(IDS_STARTER_PACK_PAGE_EMPTY_QUERY_MATCH_TEXT);
    match.contents_class = {{0, ACMatchClassification::DIM}};
  } else {
    match.contents = text;
    match.contents_class = {{0, ACMatchClassification::NONE}};
    match.subtypes.insert(omnibox::SUBTYPE_CONTEXTUAL_SEARCH);
  }

  // These are necessary to avoid the omnibox dropping out of keyword mode.
  match.keyword = input_keyword_;
  match.transition = ui::PAGE_TRANSITION_KEYWORD;
  match.allowed_to_be_default_match = true;

  matches_.push_back(match);
}
