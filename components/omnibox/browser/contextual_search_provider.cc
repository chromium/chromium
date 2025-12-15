// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/contextual_search_provider.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/lens/proto/server/lens_overlay_response.pb.h"
#include "components/omnibox/browser/actions/contextual_search_action.h"
#include "components/omnibox/browser/actions/omnibox_action_concepts.h"
#include "components/omnibox/browser/autocomplete_enums.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_debouncer.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/base_search_provider.h"
#include "components/omnibox/browser/lens_suggest_inputs_utils.h"
#include "components/omnibox/browser/match_compare.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/page_classification_functions.h"
#include "components/omnibox/browser/remote_suggestions_service.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/omnibox/browser/suggestion_group_util.h"
#include "components/omnibox/browser/zero_suggest_provider.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

// The internal default verbatim match relevance.
constexpr int kDefaultVerbatimMatchRelevance = 1500;

// Creates a URL to drive the lens API to open the side panel lens for 'whole
// page mode' with the given `query`.
GURL ComputeDestinationUrlForLensQueryText(
    const std::u16string& query,
    const TemplateURLService* turl_service) {
  CHECK(!query.empty());
  // This algorithm was reverse engineered as there's no documentation. So it's
  // very possible some of this is unnecessary or even wrong. Used these as
  // references:
  // - `ContextualSearchProvider::AddDefaultVerbatimMatch()`
  // - `SearchSuggestionParser::SuggestResult::SuggestResult()`
  // - `BaseSearchProvider::CreateSearchSuggestion()`.
  // TODO(b/403629222): Once we have a lens API that accepts a query string
  //   instead of codifying it within a GURL, we may be able to bypass
  //   `ComputeDestinationUrlForLensQueryText()`.
  auto search_terms_args =
      std::make_unique<TemplateURLRef::SearchTermsArgs>(query);
  search_terms_args->request_source = SearchTermsData::RequestSource::SEARCHBOX;
  search_terms_args->original_query = query;
  // 0 isn't even one of the valid enum value. Valid enum values are -2 and -1.
  search_terms_args->accepted_suggestion = 0;
  search_terms_args->append_extra_query_params_from_command_line = true;
  const TemplateURLRef& search_url =
      turl_service->GetDefaultSearchProvider()->url_ref();
  const SearchTermsData& search_terms_data = turl_service->search_terms_data();
  return GURL(
      search_url.ReplaceSearchTerms(*search_terms_args, search_terms_data));
}

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

// Helper to determine which matches to show. Since this is the primary
// contributor to this provider's complexity, it's easier to manage when
// centralized than distributed.
struct EligibleMatchesAndActions {
  EligibleMatchesAndActions(const AutocompleteInput& input,
                            const TemplateURL* template_url,
                            AutocompleteProviderClient* client) {
    // - Hide toolbelt in realbox.
    // - Check feature/params for zero and typed inputs.
    // - Hide toolbelt if user has disabled the context menu option.
    // - Check feature param for removing toolbelt when in keyword mode.
    const auto& toolbelt_config = omnibox_feature_configs::Toolbelt::Get();
    toolbelt =
        input.current_page_classification() !=
            metrics::OmniboxEventProto::NTP_REALBOX &&
        toolbelt_config.enabled &&
        (toolbelt_config.keep_toolbelt_after_input || input.IsZeroSuggest()) &&
        client->GetPrefs()->GetBoolean(omnibox::kShowSearchTools) &&
        (toolbelt_config.keep_toolbelt_in_keyword_mode || !template_url);

    // - Restricted to DSE google, which is already checked in
    //   `client->IsLensEnabled()`.
    // - Not restricted by locale.
    // - `LensEntrypointEligible()` restricts lens to web & SRP.
    // - Unlike `lens_entry_match`, `toolbelt_lens` is not restricted to zero
    //   inputs.
    toolbelt_lens =
        toolbelt &&
        ToolbeltActionEligible(
            input, client, toolbelt_config.show_lens_action_on_non_ntp,
            toolbelt_config.show_lens_action_on_ntp, std::nullopt) &&
        (toolbelt_config.always_include_lens_action ||
         LensEntrypointEligible(input, client));

    // When the AIM page action is enabled, we need to suppress the AIM toolbelt
    // action in order to ensure that there's at most one AIM entrypoint shown
    // in the Omnibox.
    bool is_aim_page_action_enabled =
        OmniboxFieldTrial::IsAimOmniboxEntrypointEnabled(
            client->GetAimEligibilityService());
    toolbelt_ai_mode =
        toolbelt &&
        ToolbeltActionEligible(
            input, client, toolbelt_config.show_ai_mode_action_on_non_ntp,
            toolbelt_config.show_ai_mode_action_on_ntp,
            template_url_starter_pack_data::StarterPackId::kAiMode) &&
        !is_aim_page_action_enabled;

    toolbelt_history =
        toolbelt &&
        ToolbeltActionEligible(
            input, client, toolbelt_config.show_history_action_on_non_ntp,
            toolbelt_config.show_history_action_on_ntp,
            template_url_starter_pack_data::StarterPackId::kHistory);

    toolbelt_bookmarks =
        toolbelt &&
        ToolbeltActionEligible(
            input, client, toolbelt_config.show_bookmarks_action_on_non_ntp,
            toolbelt_config.show_bookmarks_action_on_ntp,
            template_url_starter_pack_data::StarterPackId::kBookmarks);

    toolbelt_tabs =
        toolbelt &&
        ToolbeltActionEligible(
            input, client, toolbelt_config.show_tabs_action_on_non_ntp,
            toolbelt_config.show_tabs_action_on_ntp,
            template_url_starter_pack_data::StarterPackId::kTabs);

    // Hide toolbelt if it would be empty.
    toolbelt =
        toolbelt && (toolbelt_lens || toolbelt_ai_mode || toolbelt_history ||
                     toolbelt_bookmarks || toolbelt_tabs);

    // - Check feature/params.
    // - Restricted to DSE google, which is already checked in
    //   `client->IsLensEnabled()`.
    // - Not restricted by locale.
    // - `LensEntrypointEligible()` restricts lens to web & SRP.
    // - Unlike `toolbelt_lens`, `lens_entry_match` is restricted to zero
    //   inputs. `lens_entry_match`, `toolbelt_lens` is not restricted to zero
    //   inputs.
    // - Only shown if toolbelt lens not shown.
    // - Only shown if "lens search chip" (Omnibox Next) is not enabled.
    const auto& contextual_search_config =
        omnibox_feature_configs::ContextualSearch::Get();
    lens_entry_match =
        contextual_search_config.show_open_lens_action && !toolbelt_lens &&
        input.IsZeroSuggest() && LensEntrypointEligible(input, client) &&
        !client->IsOmniboxNextFeatureParamEnabled("ShowLensSearchChip");

    // - Check feature/params.
    // - Disabled if either `toolbelt` or `contextual_search_config` are shown.
    //   They are not compatible. Enabling this in parallel will require
    //   splitting the provider or making them play nicely.
    // - Shown only when the user is in '@page' scope.
    // - Hidden in incognito.
    page_verbatim = !toolbelt_config.enabled &&
                    !contextual_search_config.show_open_lens_action &&
                    template_url &&
                    template_url->starter_pack_id() ==
                        template_url_starter_pack_data::StarterPackId::kPage &&
                    !client->IsOffTheRecord();

    // - Same base requirements as `page_verbatim`
    // - Hidden on zero input.
    page_suggestions = page_verbatim && !input.text().empty() &&
                       !input.omit_asynchronous_matches();
  }

  // Show on web & SRP, but not NTP.
  // Http, https, & local files are allowed but not other local schemes.
  // Do not show if Lens is already opened.
  static bool LensEntrypointEligible(const AutocompleteInput& input,
                                     AutocompleteProviderClient* client) {
    return (omnibox::IsOtherWebPage(input.current_page_classification()) ||
            omnibox::IsSearchResultsPage(
                input.current_page_classification())) &&
           (input.current_url().SchemeIsHTTPOrHTTPS() ||
            input.current_url().SchemeIs(url::kFileScheme)) &&
           client->IsLensEnabled() && client->AreLensEntrypointsVisible();
  }

  // - Show on non-NTP depending on finch param passed in via
  //   `enabled_non_ntp`
  // - Show on NTP depending on finch param passed in via `enabled_ntp`
  // - Show only if corresponding starter pack is enabled. `starter_pack_id`
  //   is `nullopt` when the action is not associated with a starter pack.
  static bool ToolbeltActionEligible(const AutocompleteInput& input,
                                     AutocompleteProviderClient* client,
                                     bool enabled_non_ntp,
                                     bool enabled_ntp,
                                     std::optional<int> starter_pack_id) {
    // Only show on NTP if the NTP param is enabled.
    if (!enabled_ntp && input.current_page_classification() ==
                            metrics::OmniboxEventProto::
                                INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS) {
      return false;
    }

    // Only show on non-NTP if the non-NTP param is enabled.
    if (!enabled_non_ntp &&
        input.current_page_classification() !=
            metrics::OmniboxEventProto::
                INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS) {
      return false;
    }

    // If it's a starterpack action, the starterpack must be enabled.
    if (starter_pack_id.has_value()) {
      auto* turl_service = client->GetTemplateURLService();
      const TemplateURL* turl =
          turl_service->FindStarterPackTemplateURL(starter_pack_id.value());
      if (!turl || turl->is_active() != TemplateURLData::ActiveStatus::kTrue) {
        return false;
      }
    }

    return true;
  }

  // Return the toolbelt actions that are eligible.
  std::vector<scoped_refptr<OmniboxAction>> GetToolbeltActions(
      const AutocompleteInput& input,
      const TemplateURLService* turl_service) const {
    CHECK(toolbelt);
    std::vector<scoped_refptr<OmniboxAction>> actions = {};

    if (toolbelt_lens) {
      // If there is no query yet, trigger the overlay CSB so the user can start
      // creating their input (text & page selection). Otherwise, if the user
      // has already formed a textual input, bypass the overlay CSB and trigger
      // the lens side panel directly. This will unfortunately prevent the user
      // from making a page selection. Treat on focus inputs like empty inputs;
      // it's unlikely the user wants to query with the page URL.
      if (input.IsZeroSuggest() || input.text().empty()) {
        actions.push_back(
            base::MakeRefCounted<ContextualSearchOpenLensAction>());
      } else {
        GURL url =
            ComputeDestinationUrlForLensQueryText(input.text(), turl_service);
        actions.push_back(
            base::MakeRefCounted<ContextualSearchFulfillmentAction>(
                url, AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED, false));
      }
    }
    if (toolbelt_ai_mode) {
      actions.push_back(base::MakeRefCounted<StarterPackAiModeAction>());
    }
    if (toolbelt_history) {
      actions.push_back(base::MakeRefCounted<StarterPackHistoryAction>());
    }
    if (toolbelt_bookmarks) {
      actions.push_back(base::MakeRefCounted<StarterPackBookmarksAction>());
    }
    if (toolbelt_tabs) {
      actions.push_back(base::MakeRefCounted<StarterPackTabsAction>());
    }

    // `toolbelt` should be set false if it would be empty.
    CHECK(!actions.empty());
    return actions;
  }

  bool toolbelt;
  bool toolbelt_lens;
  bool toolbelt_ai_mode;
  bool toolbelt_history;
  bool toolbelt_bookmarks;
  bool toolbelt_tabs;
  bool lens_entry_match;
  bool page_verbatim;
  bool page_suggestions;
};

}  // namespace

void ContextualSearchProvider::Start(const AutocompleteInput& input,
                                     bool minimal_changes) {
  TRACE_EVENT0("omnibox", "ContextualSearchProvider::Start");
  // Clear the cached results to remove the page search action matches. Also,
  // matches the behavior of the `ZeroSuggestProvider`.
  Stop(AutocompleteStopReason::kClobbered);

  // Determine keyword (may be nullptr, a starter pack, or some other keyword).
  AutocompleteInput keyword_input = input;
  const TemplateURL* template_url =
      input.prefer_keyword()
          ? AutocompleteInput::GetSubstitutingTemplateURLForInput(
                client()->GetTemplateURLService(), &keyword_input)
          : nullptr;

  const EligibleMatchesAndActions eligibility(input, template_url, client());

  if (eligibility.toolbelt) {
    AddToolbeltMatch(keyword_input,
                     eligibility.GetToolbeltActions(
                         keyword_input, client()->GetTemplateURLService()));
  }

  if (eligibility.lens_entry_match) {
    AddLensEntrypointMatch(keyword_input);
  }

  if (eligibility.page_verbatim) {
    // `template_url` can't be nullptr here; see `page_verbatim` assignment.
    DCHECK(template_url);
    input_keyword_ = template_url->keyword();
    AddDefaultVerbatimMatch(input);
  }

  if (eligibility.page_suggestions) {
    done_ = false;
    AddDefaultVerbatimMatch(keyword_input);
    StartSuggestRequest(std::move(keyword_input));
  }
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

bool ContextualSearchProvider::HasToolbeltLensAction() const {
  return std::ranges::any_of(matches_, [](const auto& match) {
    return match.IsToolbelt() &&
           match.HasAction(OmniboxActionId::CONTEXTUAL_SEARCH_OPEN_LENS);
  });
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
    std::optional<std::string> response_body) {
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

void ContextualSearchProvider::AddLensEntrypointMatch(
    const AutocompleteInput& input) {
  // This match is effectively a pedal that doesn't require any query matching.
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
      base::UTF8ToUTF16(url_formatter::StripWWW(input.current_url().GetHost()));
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

void ContextualSearchProvider::AddToolbeltMatch(
    const AutocompleteInput& input,
    std::vector<scoped_refptr<OmniboxAction>> actions) {
  AutocompleteMatch match(this, omnibox::kToolbeltRelevance, false,
                          AutocompleteMatchType::NULL_RESULT_MESSAGE);
  match.transition = ui::PAGE_TRANSITION_GENERATED;
  match.suggest_type = omnibox::SuggestType::TYPE_NATIVE_CHROME;
  match.suggestion_group_id = omnibox::GroupId::GROUP_SEARCH_TOOLBELT;
  match.fill_into_edit = input.text();

  match.description = l10n_util::GetStringUTF16(IDS_OMNIBOX_TOOLBELT_LABEL);
  if (!match.description.empty()) {
    match.description_class = {{0, ACMatchClassification::TOOLBELT}};
  }

  match.actions = actions;
  matches_.push_back(match);
}

const TemplateURL* ContextualSearchProvider::GetKeywordTemplateURL() const {
  return client()->GetTemplateURLService()->GetTemplateURLForKeyword(
      input_keyword_);
}
