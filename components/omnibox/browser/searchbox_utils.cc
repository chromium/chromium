// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/searchbox_utils.h"

#include <string>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "build/build_config.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/navigation_metrics/navigation_metrics.h"
#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/actions/omnibox_action_concepts.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#if !BUILDFLAG(IS_IOS)
#include "components/omnibox/browser/geolocation_header_service.h"
#endif  // !BUILDFLAG(IS_IOS)
#include "components/omnibox/browser/history_fuzzy_provider.h"
#include "components/omnibox/browser/history_url_provider.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_event_global_tracker.h"
#include "components/omnibox/browser/omnibox_log.h"
#include "components/omnibox/browser/omnibox_logging_utils.h"
#include "components/omnibox/browser/page_classification_functions.h"
#include "components/omnibox/browser/verbatim_match.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/core/session_id.h"
#include "net/cookies/cookie_util.h"
#include "ui/base/page_transition_types.h"

namespace searchbox {

void OpenMatch(AutocompleteController* autocomplete_controller,
               OmniboxClient* client,
               OmniboxPopupSelection selection,
               AutocompleteMatch match,
               WindowOpenDisposition disposition,
               base::TimeTicks searchbox_focused_timestamp,
               base::TimeTicks first_modification_timestamp,
               base::TimeTicks match_selection_timestamp) {
  const base::TimeTicks now = base::TimeTicks::Now();

  // TODO(crbug.com/530254690): Use the input associated with the result
  //  holding the match.
  const AutocompleteInput& input = autocomplete_controller->input();
  const AutocompleteResult& result = autocomplete_controller->result();

  // If the user is executing an action, this will be non-null and some match
  // opening and metrics behavior will be adjusted accordingly.
  OmniboxAction* action = nullptr;
  if (selection.state == OmniboxPopupSelection::NORMAL &&
      match.takeover_action) {
    DCHECK_NE(match_selection_timestamp, base::TimeTicks());
    action = match.takeover_action.get();
  } else if (selection.IsAction()) {
    DCHECK_LT(selection.action_index, match.actions.size());
    action = match.actions[selection.action_index].get();
  }

  // Invalid URLs such as chrome://history can end up here, but that's okay
  // if the user is executing an action instead of navigating to the URL.
  if (!match.destination_url.is_valid() && !action) {
    return;
  }

  // NULL_RESULT_MESSAGE matches are informational only and cannot be acted
  // upon. Immediately return when attempting to open one.
  if (match.type == AutocompleteMatchType::NULL_RESULT_MESSAGE && !action) {
    return;
  }

  // Switch the window disposition to SWITCH_TO_TAB for open tab matches that
  // originated while in keyword mode, or for tab switch actions.
  const bool is_open_tab_match =
      match.from_keyword && match.type == AutocompleteMatchType::OPEN_TAB;
  const bool is_tab_switch_action =
      action && action->ActionId() == OmniboxActionId::TAB_SWITCH;
  if (is_open_tab_match || is_tab_switch_action) {
    disposition = WindowOpenDisposition::SWITCH_TO_TAB;
  }

  base::TimeDelta elapsed_time_since_user_first_modified_omnibox =
      now - first_modification_timestamp;
  autocomplete_controller
      ->UpdateMatchDestinationURLWithAdditionalSearchboxStats(
          elapsed_time_since_user_first_modified_omnibox, &match);

  GURL destination_url = action ? action->getUrl() : match.destination_url;

  omnibox::RecordActionShownForAllActions(result, selection);
  HistoryFuzzyProvider::RecordOpenMatchMetrics(result, match);

  // TODO(crbug.com/530290300): This should no longer be possible once
  //  selections are tied to source result. Eliminate and simplify below.
  bool dropdown_ignored = selection.line >= result.size();

  ACMatches fake_single_entry_matches;
  AutocompleteResult fake_single_entry_result;
  if (dropdown_ignored) {
    fake_single_entry_matches.push_back(match);
    fake_single_entry_result.AppendMatches(fake_single_entry_matches);
  }

  const metrics::OmniboxEventProto::PageClassification page_classification =
      client->GetPageClassification(/*is_prefetch=*/false);

  base::TimeDelta elapsed_time_since_last_change_to_default_match =
      now - autocomplete_controller->last_time_default_match_changed();

  // These elapsed times don't really make sense for matches that come from
  // omnibox focus, i.e. zero suggest, because the user did not modify the
  // omnibox. So for those we set the elapsed times to something that will be
  // ignored by metrics_log.cc.
  const base::TimeDelta default_time_delta = base::Milliseconds(-1);
  if (input.IsZeroSuggest()) {
    elapsed_time_since_user_first_modified_omnibox = default_time_delta;
    elapsed_time_since_last_change_to_default_match = default_time_delta;
  }
  base::TimeDelta elapsed_time_since_user_focused_searchbox =
      default_time_delta;
  if (!searchbox_focused_timestamp.is_null()) {
    elapsed_time_since_user_focused_searchbox =
        now - searchbox_focused_timestamp;
    // Only record focus to open time when a focus actually happened (as
    // opposed to, say, dragging a link onto the omnibox).
    omnibox::LogFocusToOpenTime(
        elapsed_time_since_user_focused_searchbox, input.IsZeroSuggest(),
        page_classification, match,
        selection.IsAction() ? selection.action_index : -1);
  }

  const std::u16string user_text =
      input.IsZeroSuggest() ? std::u16string() : input.text();
  const size_t completed_length = match.allowed_to_be_default_match
                                      ? match.inline_autocompletion.length()
                                      : std::u16string::npos;
  const bool is_incognito =
      autocomplete_controller->autocomplete_provider_client()->IsOffTheRecord();

  OmniboxLog log(
      user_text,

      // TODO(crbug.com/530295819): Would need signal from UI to support this.
      /*just_deleted_text=*/false, input.type(),

      // TODO(crbug.com/529914184): Track the keyword mode entry method.
      input.in_keyword_mode(),
      /*entry_method=*/metrics::OmniboxEventProto::INVALID,

      /*is_popup_open=*/true,
      dropdown_ignored ? OmniboxPopupSelection(0) : selection, disposition,
      /*is_paste_and_go=*/false, SessionID::InvalidValue(), page_classification,
      elapsed_time_since_user_first_modified_omnibox, completed_length,
      elapsed_time_since_last_change_to_default_match,
      dropdown_ignored ? fake_single_entry_result : result, destination_url,
      is_incognito, input.IsZeroSuggest(), match.session);
  DCHECK(dropdown_ignored ||
         (log.elapsed_time_since_user_first_modified_omnibox >=
          log.elapsed_time_since_last_change_to_default_match))
      << "We should've got the notification that the user modified the "
      << "searchbox text at same time or before the most recent time the "
      << "default match changed.";
  log.elapsed_time_since_user_focused_omnibox =
      elapsed_time_since_user_focused_searchbox;
  log.input_state = input.input_state();
  log.ukm_source_id = client->GetUKMSourceId();

  if ((disposition == WindowOpenDisposition::CURRENT_TAB) &&
      client->CurrentPageExists()) {
    // If we know the destination is being opened in the current tab,
    // we can easily get the tab ID.  (If it's being opened in a new
    // tab, we don't know the tab ID yet.)
    log.tab_id = client->GetSessionID();
  }

  autocomplete_controller->AddProviderAndTriggeringLogs(&log);
  client->OnURLOpenedFromOmnibox(&log);
  OmniboxEventGlobalTracker::GetInstance()->OnURLOpened(&log);

  // TODO(crbug.com/531799956): Factor out some UMA metrics calls for reuse.
#if !BUILDFLAG(IS_IOS)
  if (auto* geolocation_header_service =
          autocomplete_controller->autocomplete_provider_client()
              ->GetGeolocationHeaderService()) {
    geolocation_header_service->RecordInlineLocationSuggestionClicked(match);
  }
#endif  // !BUILDFLAG(IS_IOS)

  TemplateURLService* template_url_service = client->GetTemplateURLService();
  if (action) {
    const int enter_starter_pack_id = client->ExecuteAction(
        action, disposition, match_selection_timestamp,
        *(autocomplete_controller->autocomplete_provider_client()));
    if (enter_starter_pack_id != 0 && template_url_service) {
      // TODO(crbug.com/531796992): Signal keyword mode entry if any actions
      //  do return a nonzero starter pack ID. May be deprecated; if only the
      //  toolbelt feature was using this, it can be simplified/removed.
      template_url_starter_pack_data::StarterPackId starter_pack_id =
          static_cast<template_url_starter_pack_data::StarterPackId>(
              enter_starter_pack_id);
      if (template_url_service->FindStarterPackTemplateURL(starter_pack_id)) {
        return;
      }
    }
  } else {
    bookmarks::BookmarkModel* bookmark_model = client->GetBookmarkModel();
    if (bookmark_model && bookmark_model->IsBookmarked(destination_url)) {
      client->OnBookmarkLaunched();
    }

    if (template_url_service) {
      if (template_url_service->IsSearchResultsPageFromDefaultSearchProvider(
              destination_url)) {
        RecordDefaultSearchProviderSearchMetrics(is_incognito);
      }

      TemplateURL* template_url = match.GetTemplateURL(template_url_service);
      if (template_url) {
        AutocompleteMatch::LogSearchEngineUsed(match, template_url_service);

        if (ui::PageTransitionTypeIncludingQualifiersIs(
                match.transition, ui::PAGE_TRANSITION_KEYWORD) ||
            match.provider->type() ==
                AutocompleteProvider::TYPE_UNSCOPED_EXTENSION) {
          base::RecordAction(base::UserMetricsAction("AcceptedKeyword"));
          // OEM state: EmitAcceptedKeywordSuggestionHistogram
          template_url_service->IncrementUsageCount(template_url);

          if (template_url->type() == TemplateURL::OMNIBOX_API_EXTENSION) {
            client->ProcessExtensionMatch(input.text(), template_url, match,
                                          disposition);
            return;
          }
        }
      } else {
        if (ui::PageTransitionTypeIncludingQualifiersIs(
                match.transition, ui::PAGE_TRANSITION_TYPED)) {
          navigation_metrics::RecordOmniboxURLNavigation(destination_url);
        }

        if (ui::PageTransitionTypeIncludingQualifiersIs(
                match.transition, ui::PAGE_TRANSITION_TYPED) ||
            ui::PageTransitionTypeIncludingQualifiersIs(
                match.transition, ui::PAGE_TRANSITION_LINK)) {
          net::cookie_util::RecordCookiePortOmniboxHistograms(destination_url);
        }
      }
    }

    // TODO(crbug.com/531810530): Ensure this doesn't need to be plumbed in
    //  as with OmniboxEditModel::OpenMatch. Might be refactored there too.
    GURL alternate_nav_url = AutocompleteResult::ComputeAlternateNavUrl(
        input, match, autocomplete_controller->autocomplete_provider_client());

    AutocompleteInput alternate_input(
        input.text(), page_classification, client->GetSchemeClassifier(),
        client->ShouldDefaultTypedNavigationsToHttps(), 0, false);
    alternate_input.set_current_url(client->GetURL());
    alternate_input.set_current_title(client->GetTitle());

    AutocompleteMatch alternative_nav_match = VerbatimMatchForInput(
        autocomplete_controller->history_url_provider(),
        autocomplete_controller->autocomplete_provider_client(),
        alternate_input, alternate_nav_url, false);

    client->OnAutocompleteAccept(
        destination_url, match.post_content.get(), disposition,
        ui::PageTransitionFromInt(match.transition |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
        match.type, match_selection_timestamp,
        input.added_default_scheme_to_typed_url(),
        input.typed_url_had_http_scheme() &&
            match.type == AutocompleteMatchType::URL_WHAT_YOU_TYPED,
        input.text(), match, alternative_nav_match);
  }
}

void RecordDefaultSearchProviderSearchMetrics(bool is_off_the_record) {
  base::RecordAction(
      base::UserMetricsAction("OmniboxDestinationURLIsSearchOnDSP"));
  base::UmaHistogramBoolean("Omnibox.Search.OffTheRecord", is_off_the_record);
}

}  // namespace searchbox
