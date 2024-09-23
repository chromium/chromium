// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_LOG_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_LOG_H_

#include <stddef.h>

#include <string>

#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
#include "components/omnibox/browser/omnibox_triggered_feature_service.h"
#include "components/sessions/core/session_id.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

class AutocompleteResult;

// The data to log (via the metrics service) when the user selects an item from
// the omnibox popup.
struct OmniboxLog {
  OmniboxLog(const std::u16string& text,
             bool just_deleted_text,
             metrics::OmniboxInputType input_type,
             bool in_keyword_mode,
             metrics::OmniboxEventProto::KeywordModeEntryMethod entry_method,
             bool is_popup_open,
             OmniboxPopupSelection selection,
             WindowOpenDisposition disposition,
             bool is_paste_and_go,
             SessionID tab_id,
             metrics::OmniboxEventProto::PageClassification
                 current_page_classification,
             base::TimeDelta elapsed_time_since_user_first_modified_omnibox,
             size_t completed_length,
             base::TimeDelta elapsed_time_since_last_change_to_default_match,
             const AutocompleteResult& result,
             const GURL& destination_url,
             bool is_incognito);
  ~OmniboxLog();

  // The user's input text in the omnibox.
  std::u16string text;

  // Whether the user deleted text immediately before selecting an omnibox
  // suggestion.  This is usually the result of pressing backspace or delete.
  bool just_deleted_text;

  // The detected type of the user's input.
  metrics::OmniboxInputType input_type;

  // Whether the Omnibox was in keyword mode when the user selected a
  // suggestion.
  bool in_keyword_mode;

  // Preserves the method that the user used to enter keyword mode. If
  // |in_keyword_mode| is false, this should be INVALID.
  metrics::OmniboxEventProto::KeywordModeEntryMethod keyword_mode_entry_method;

  // True if the popup is open.
  bool is_popup_open;

  // Contains the selection used to open a match or take an action. This
  // includes the index of the item selected in the dropdown list (or 0 if the
  // dropdown is closed and therefore there is only one implicit suggestion).
  OmniboxPopupSelection selection;

  // The disposition used to open the match. Currently, only SWITCH_TO_TAB
  // is relevant to the log; all other dispositions are treated identically.
  WindowOpenDisposition disposition;

  // True if this is a paste-and-search or paste-and-go omnibox interaction.
  // (The codebase refers to both these types as paste-and-go.)
  bool is_paste_and_go;

  // ID of the tab the selected autocomplete suggestion was opened in. Set to
  // SessionID::InvalidValue() if we haven't yet determined the destination tab.
  SessionID tab_id;

  // The type of page (e.g., new tab page, regular web page) that the
  // user was viewing before going somewhere with the omnibox.
  metrics::OmniboxEventProto::PageClassification current_page_classification;

  // The amount of time since the user first began modifying the text
  // in the omnibox.  If at some point after modifying the text, the
  // user reverts the modifications (thus seeing the current web
  // page's URL again), then writes in the omnibox again, this time
  // delta should be computed starting from the second series of
  // modifications.  If we somehow skipped the logic to record
  // the time the user began typing (this should only happen in
  // unit tests), this elapsed time is set to -1 milliseconds.
  base::TimeDelta elapsed_time_since_user_first_modified_omnibox;

  // The number of extra characters the user would have to manually type
  // if they were not given the opportunity to select this match.  Only
  // set for matches that are allowed to be the default match (i.e., are
  // inlineable).  Set to std::u16string::npos if the match is not allowed
  // to be the default match.
  size_t completed_length;

  // The amount of time since the last time the default (i.e., inline)
  // match changed.  This will certainly be less than
  // elapsed_time_since_user_first_modified_omnibox.  Measuring this
  // may be inappropriate in some cases (e.g., if editing is not in
  // progress).  In such cases, it's set to -1 milliseconds.
  base::TimeDelta elapsed_time_since_last_change_to_default_match;

  // Result set.
  const raw_ref<const AutocompleteResult> result;

  // Diagnostic information from providers.  See
  // AutocompleteController::AddProviderAndTriggeringLogs() and
  // AutocompleteProvider::AddProviderInfo().
  ProvidersInfo providers_info;

  // The features that have been triggered (see
  // OmniboxTriggeredFeatureService::Feature).
  OmniboxTriggeredFeatureService::Features features_triggered;
  OmniboxTriggeredFeatureService::Features features_triggered_in_session;

  // Whether the omnibox input is a search query that is started
  // by clicking on a image tile. Currently only used on Android.
  bool is_query_started_from_tile = false;

  // The final computed URL for the navigation. This does not always match the
  // destination URL within |result| as the match URLs are computed to add
  // additional data from the client.
  GURL final_destination_url;

  // Whether the item selection happened on an off-the-record/incognito profile.
  // This is used to disable logging of scoring signals in incognito mode.
  bool is_incognito;

  // The preferred steady state (unfocused) omnibox position. Only logged on
  // iOS phones.
  metrics::OmniboxEventProto::OmniboxPosition steady_state_omnibox_position;

  // The UKM source id for the last committed navigation in the top frame.
  ukm::SourceId ukm_source_id;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_LOG_H_
