// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/searchbox_utils.h"

#include <string>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "build/build_config.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/navigation_metrics/navigation_metrics.h"
#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/actions/omnibox_action_concepts.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#if !BUILDFLAG(IS_IOS)
#include "components/omnibox/browser/geolocation_header_service.h"
#endif  // !BUILDFLAG(IS_IOS)
#include "base/metrics/histogram_functions.h"
#include "components/omnibox/browser/history_fuzzy_provider.h"
#include "components/omnibox/browser/history_url_provider.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_event_global_tracker.h"
#include "components/omnibox/browser/omnibox_log.h"
#include "components/omnibox/browser/omnibox_logging_utils.h"
#include "components/omnibox/browser/omnibox_metrics_constants.h"
#include "components/omnibox/browser/page_classification_functions.h"
#include "components/omnibox/browser/verbatim_match.h"
#include "components/search_engines/template_url.h"
#include "components/sessions/core/session_id.h"
#include "net/cookies/cookie_util.h"
#include "ui/base/page_transition_types.h"

using metrics::OmniboxEventProto;

namespace {

void ClassifyString(OmniboxClient* client,
                    const std::u16string& text,
                    AutocompleteMatch* match,
                    GURL* alternate_nav_url) {
  DCHECK(match);
  client->GetAutocompleteClassifier()->Classify(
      text, false, false, client->GetPageClassification(/*is_prefetch=*/false),
      match, alternate_nav_url);
}

}  // namespace

namespace searchbox {

InteractionMetricsTracker::InteractionMetricsTracker() = default;

InteractionMetricsTracker::~InteractionMetricsTracker() = default;

void InteractionMetricsTracker::FocusChanged(bool focused) {
  if (focused) {
    last_omnibox_focus_ = base::TimeTicks::Now();
    focus_resulted_in_navigation_ = false;
  } else {
    if (!last_omnibox_focus_.is_null()) {
      base::UmaHistogramBoolean("Omnibox.FocusResultedInNavigation",
                                focus_resulted_in_navigation_);
    }
    last_omnibox_focus_ = base::TimeTicks();
  }
}

AutocompleteMatch GenerateDotComMatch(
    OmniboxClient* client,
    AutocompleteController* autocomplete_controller,
    const AutocompleteInput& original_input,
    const std::u16string& text_for_desired_tld_navigation,
    AutocompleteInput* generated_input) {
  AutocompleteInput input(
      text_for_desired_tld_navigation, original_input.cursor_position(), "com",
      original_input.current_page_classification(),
      client->GetSchemeClassifier(),
      client->ShouldDefaultTypedNavigationsToHttps(), 0, false);
  input.set_prevent_inline_autocomplete(
      original_input.prevent_inline_autocomplete());
  input.set_in_keyword_mode(original_input.in_keyword_mode());
  input.set_allow_exact_keyword_match(
      original_input.allow_exact_keyword_match());
  input.set_omit_asynchronous_matches(
      original_input.omit_asynchronous_matches());
  input.set_focus_type(original_input.focus_type());

  if (generated_input) {
    *generated_input = input;
  }

  AutocompleteMatch match = VerbatimMatchForInput(
      autocomplete_controller->history_url_provider(),
      autocomplete_controller->autocomplete_provider_client(), input,
      input.canonicalized_url(), false);

  base::UmaHistogramBoolean("Omnibox.Search.CtrlEnter.ResolvedAsUrl",
                            match.destination_url.is_valid());
  return match;
}

void OpenMatch(
    AutocompleteController* autocomplete_controller,
    OmniboxClient* client,
    const AutocompleteInput& input,
    OmniboxPopupSelection selection,
    AutocompleteMatch match,
    WindowOpenDisposition disposition,
    const InteractionMetricsTracker& metrics_tracker,
    OmniboxEventProto::KeywordModeEntryMethod keyword_mode_entry_method,
    const std::u16string& pasted_text) {
  const base::TimeTicks now = base::TimeTicks::Now();
  const AutocompleteResult& result = autocomplete_controller->result();

  // If the user is executing an action, this will be non-null and some match
  // opening and metrics behavior will be adjusted accordingly.
  OmniboxAction* action = nullptr;
  if (selection.state == OmniboxPopupSelection::NORMAL &&
      match.takeover_action) {
    DCHECK_NE(metrics_tracker.match_selection_timestamp(), base::TimeTicks());
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
      now - metrics_tracker.time_user_first_modified_omnibox();
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

  const OmniboxEventProto::PageClassification page_classification =
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
  if (!metrics_tracker.last_omnibox_focus().is_null()) {
    elapsed_time_since_user_focused_searchbox =
        now - metrics_tracker.last_omnibox_focus();
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
      /*entry_method=*/OmniboxEventProto::INVALID,

      /*is_popup_open=*/true,
      dropdown_ignored ? OmniboxPopupSelection(0) : selection, disposition,
      /*is_paste_and_go=*/!pasted_text.empty(), SessionID::InvalidValue(),
      page_classification, elapsed_time_since_user_first_modified_omnibox,
      completed_length, elapsed_time_since_last_change_to_default_match,
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

  RecordSuggestionUsedMetrics(match);

  omnibox::LogIPv4PartsCount(user_text, destination_url, completed_length);

  client->OnURLOpenedFromOmnibox(&log);
  OmniboxEventGlobalTracker::GetInstance()->OnURLOpened(&log);

#if !BUILDFLAG(IS_IOS)
  if (auto* geolocation_header_service =
          autocomplete_controller->autocomplete_provider_client()
              ->GetGeolocationHeaderService()) {
    geolocation_header_service->MaybeRecordInlineLocationSuggestionClicked(
        match);
  }
#endif  // !BUILDFLAG(IS_IOS)

  TemplateURLService* template_url_service = client->GetTemplateURLService();
  TemplateURL* template_url = match.GetTemplateURL(template_url_service);
  if (template_url) {
    // `match` is a Search navigation or a URL navigation in keyword mode; log
    // search engine usage metrics.
    AutocompleteMatch::LogSearchEngineUsed(match, template_url_service);

    if (ui::PageTransitionTypeIncludingQualifiersIs(
            match.transition, ui::PAGE_TRANSITION_KEYWORD) ||
        match.provider->type() ==
            AutocompleteProvider::TYPE_UNSCOPED_EXTENSION) {
      // User is in keyword mode or accepted an unscoped extension suggestion,
      // increment usage count for the keyword.
      searchbox::EmitAcceptedKeywordSuggestionHistogram(
          keyword_mode_entry_method, template_url);
      template_url_service->IncrementUsageCount(template_url);

      // Notify the extension of the selected input, but ignore if the selection
      // corresponds to an action created by an extension in unscoped mode.
      if (template_url->type() == TemplateURL::OMNIBOX_API_EXTENSION &&
          !action) {
        client->ProcessExtensionMatch(input.text(), template_url, match,
                                      disposition);
        // Avoid calling `OmniboxClient::OnAutocompleteAccept()`. The extension
        // was notfied of the accepted input and will handle the navigation.
        return;
      }
    } else {
      DCHECK(ui::PageTransitionTypeIncludingQualifiersIs(
                 match.transition, ui::PAGE_TRANSITION_GENERATED) ||
             ui::PageTransitionTypeIncludingQualifiersIs(
                 match.transition, ui::PAGE_TRANSITION_RELOAD));
      // NOTE: We purposefully don't increment the usage count of the default
      // search engine here like we do for explicit keywords above; see comments
      // in template_url.h.
    }
  } else {
    // `match` is a URL navigation, not a search.
    // For logging the below histogram, only record uses that depend on the
    // omnibox suggestion system, i.e., TYPED navigations.  That is, exclude
    // omnibox URL interactions that are treated as reloads or link-following
    // (i.e., cut-and-paste of URLs) or paste-and-go.
    if (ui::PageTransitionTypeIncludingQualifiersIs(
            match.transition, ui::PAGE_TRANSITION_TYPED) &&
        pasted_text.empty()) {
      navigation_metrics::RecordOmniboxURLNavigation(destination_url);
    }

    // The following histograms should be recorded for both TYPED and pasted
    // URLs, but should still exclude reloads.
    if (ui::PageTransitionTypeIncludingQualifiersIs(
            match.transition, ui::PAGE_TRANSITION_TYPED) ||
        ui::PageTransitionTypeIncludingQualifiersIs(match.transition,
                                                    ui::PAGE_TRANSITION_LINK)) {
      net::cookie_util::RecordCookiePortOmniboxHistograms(destination_url);
    }
  }

  if (action) {
    client->ExecuteAction(
        action, disposition, metrics_tracker.match_selection_timestamp(),
        *(autocomplete_controller->autocomplete_provider_client()));
    return;
  }

  RecordNonActionSearchMetrics(template_url_service, match, is_incognito,
                               metrics_tracker.match_selection_timestamp());

  bookmarks::BookmarkModel* bookmark_model = client->GetBookmarkModel();
  if (bookmark_model && bookmark_model->IsBookmarked(destination_url)) {
    client->OnBookmarkLaunched();
  }

  GURL alternate_nav_url = AutocompleteResult::ComputeAlternateNavUrl(
      input, match, autocomplete_controller->autocomplete_provider_client());

  AutocompleteInput alternate_input(
      input.text(), page_classification, client->GetSchemeClassifier(),
      client->ShouldDefaultTypedNavigationsToHttps(), 0, false);
  alternate_input.set_current_url(client->GetURL());
  alternate_input.set_current_title(client->GetTitle());

  AutocompleteMatch alternative_nav_match = VerbatimMatchForInput(
      autocomplete_controller->history_url_provider(),
      autocomplete_controller->autocomplete_provider_client(), alternate_input,
      alternate_nav_url, false);

  client->OnAutocompleteAccept(
      destination_url, match.post_content.get(), disposition,
      ui::PageTransitionFromInt(match.transition |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
      match.type, metrics_tracker.match_selection_timestamp(),
      input.added_default_scheme_to_typed_url(),
      input.typed_url_had_http_scheme() &&
          match.type == AutocompleteMatchType::URL_WHAT_YOU_TYPED,
      input.text(), match, alternative_nav_match);
}

bool CanPasteAndGo(OmniboxClient* client, const std::u16string& text) {
  if (!client->IsPasteAndGoEnabled()) {
    return false;
  }

  AutocompleteMatch match;
  ClassifyString(client, text, &match, nullptr);
  return match.destination_url.is_valid();
}

void PasteAndGo(AutocompleteController* autocomplete_controller,
                OmniboxClient* client,
                const std::u16string& text,
                const InteractionMetricsTracker& metrics_tracker,
                metrics::OmniboxEventProto::KeywordModeEntryMethod
                    keyword_mode_entry_method) {
  DCHECK(CanPasteAndGo(client, text));

  AutocompleteInput input = autocomplete_controller->input();
  AutocompleteMatch match;
  GURL alternate_nav_url;
  ClassifyString(client, text, &match, &alternate_nav_url);

  GURL upgraded_url;
  if (match.type == AutocompleteMatchType::URL_WHAT_YOU_TYPED &&
      client->ShouldDefaultTypedNavigationsToHttps() &&
      AutocompleteInput::ShouldUpgradeToHttps(text, match.destination_url, 0,
                                              false, &upgraded_url)) {
    DCHECK(upgraded_url.is_valid());
    match.destination_url = upgraded_url;
    input.set_added_default_scheme_to_typed_url(true);
  } else {
    input.set_added_default_scheme_to_typed_url(false);
  }

  OpenMatch(autocomplete_controller, client, input,
            OmniboxPopupSelection(OmniboxPopupSelection::kNoMatch), match,
            WindowOpenDisposition::CURRENT_TAB, metrics_tracker,
            keyword_mode_entry_method, text);
}

void RecordNonActionSearchMetrics(TemplateURLService* template_url_service,
                                  const AutocompleteMatch& match,
                                  bool is_off_the_record,
                                  base::TimeTicks match_selection_timestamp) {
  const base::TimeTicks now = base::TimeTicks::Now();

  // Track whether the destination URL sends us to a search results page
  // using the default search provider.
  if (template_url_service &&
      template_url_service->IsSearchResultsPageFromDefaultSearchProvider(
          match.destination_url)) {
    base::RecordAction(
        base::UserMetricsAction("OmniboxDestinationURLIsSearchOnDSP"));
    base::UmaHistogramBoolean("Omnibox.Search.OffTheRecord", is_off_the_record);
  }

  if (match.destination_url.is_valid()) {
    base::UmaHistogramMicrosecondsTimes("Omnibox.InputToAcceptNonAction",
                                        now - match_selection_timestamp);
  }
}

void EmitAcceptedKeywordSuggestionHistogram(
    OmniboxEventProto::KeywordModeEntryMethod entry_method,
    const TemplateURL* turl) {
  base::RecordAction(base::UserMetricsAction("AcceptedKeyword"));
  UMA_HISTOGRAM_ENUMERATION(
      omnibox::kAcceptedKeywordSuggestionHistogram,
      static_cast<int>(entry_method),
      static_cast<int>(OmniboxEventProto::KeywordModeEntryMethod_MAX + 1));

  if (turl != nullptr) {
    base::UmaHistogramEnumeration(
        omnibox::kKeywordModeUsageByEngineTypeAcceptedHistogramName,
        turl->GetBuiltinEngineType(),
        BuiltinEngineType::KEYWORD_MODE_ENGINE_TYPE_MAX);
  }
}

void RecordSuggestionUsedMetrics(const AutocompleteMatch& match) {
  base::UmaHistogramEnumeration("Omnibox.SuggestionUsed.RichAutocompletion",
                                match.rich_autocompletion_triggered);
  LOCAL_HISTOGRAM_BOOLEAN("Omnibox.EventCount", true);
  omnibox::answer_data_parser::LogAnswerUsed(match.answer_type);
}

WindowOpenDisposition ComputeOpenDispositionFromModifiersAndLogToUma(
    bool shift,
    bool control,
    bool alt,
    bool command) {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(OpenMatchWithKeyboardModifiers)
  enum class OpenMatchWithKeyboardModifiers {
    kNoModifier = 0,
    kCtrl = 1,
    kAlt = 2,
    kCtrlAlt = 3,
    kShiftCommand = 4,
    kCtrlShiftCommand = 5,
    kAltShift = 6,
    kCtrlAltShift = 7,
    kCommand = 8,
    kCtrlCommand = 9,
    kShift = 10,
    kCtrlShift = 11,
    kMaxValue = kCtrlShift,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/omnibox/enums.xml:OpenMatchWithKeyboardModifiers)

  WindowOpenDisposition disposition = WindowOpenDisposition::CURRENT_TAB;
  OpenMatchWithKeyboardModifiers metric_value;
  if (alt && !shift) {
    metric_value = control ? OpenMatchWithKeyboardModifiers::kCtrlAlt
                           : OpenMatchWithKeyboardModifiers::kAlt;
    disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  } else if (shift && command) {
    metric_value = control ? OpenMatchWithKeyboardModifiers::kCtrlShiftCommand
                           : OpenMatchWithKeyboardModifiers::kShiftCommand;
    disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  } else if (alt && shift) {
    metric_value = control ? OpenMatchWithKeyboardModifiers::kCtrlAltShift
                           : OpenMatchWithKeyboardModifiers::kAltShift;
    disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
  } else if (command && !shift) {
    metric_value = control ? OpenMatchWithKeyboardModifiers::kCtrlCommand
                           : OpenMatchWithKeyboardModifiers::kCommand;
    disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
  } else if (shift && !alt) {
    metric_value = control ? OpenMatchWithKeyboardModifiers::kCtrlShift
                           : OpenMatchWithKeyboardModifiers::kShift;
    disposition = WindowOpenDisposition::NEW_WINDOW;
  } else {
    metric_value = control ? OpenMatchWithKeyboardModifiers::kCtrl
                           : OpenMatchWithKeyboardModifiers::kNoModifier;
  }
  base::UmaHistogramEnumeration("Omnibox.OpenMatchWithKeyboardModifiers",
                                metric_value);
  return disposition;
}

}  // namespace searchbox
