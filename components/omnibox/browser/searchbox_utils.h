// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_SEARCHBOX_UTILS_H_
#define COMPONENTS_OMNIBOX_BROWSER_SEARCHBOX_UTILS_H_

#include <string>

#include "base/time/time.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/base/window_open_disposition.h"

class AutocompleteController;
class OmniboxClient;

namespace searchbox {

// Tracks searchbox-related metrics and focus state.
class InteractionMetricsTracker {
 public:
  InteractionMetricsTracker();
  InteractionMetricsTracker(const InteractionMetricsTracker&) = delete;
  InteractionMetricsTracker& operator=(const InteractionMetricsTracker&) =
      delete;
  ~InteractionMetricsTracker();

  // Updates focus state and logs navigation metrics on kill focus.
  void FocusChanged(bool focused);

  base::TimeTicks last_omnibox_focus() const { return last_omnibox_focus_; }
  void set_last_omnibox_focus(base::TimeTicks last_omnibox_focus) {
    last_omnibox_focus_ = last_omnibox_focus;
  }

  bool focus_resulted_in_navigation() const {
    return focus_resulted_in_navigation_;
  }
  void set_focus_resulted_in_navigation(bool focus_resulted_in_navigation) {
    focus_resulted_in_navigation_ = focus_resulted_in_navigation;
  }

  base::TimeTicks time_user_first_modified_omnibox() const {
    return time_user_first_modified_omnibox_;
  }
  void set_time_user_first_modified_omnibox(
      base::TimeTicks time_user_first_modified_omnibox) {
    time_user_first_modified_omnibox_ = time_user_first_modified_omnibox;
  }

  base::TimeTicks match_selection_timestamp() const {
    return match_selection_timestamp_;
  }
  void set_match_selection_timestamp(
      base::TimeTicks match_selection_timestamp) {
    match_selection_timestamp_ = match_selection_timestamp;
  }

 private:
  // We keep track of when the user last focused on the searchbox.
  base::TimeTicks last_omnibox_focus_;

  // Indicates whether the current interaction with the searchbox resulted in
  // navigation (true), or user leaving the searchbox without taking any action
  // (false).
  // The value is initialized when the searchbox receives focus and available
  // for use when the focus is about to be cleared.
  bool focus_resulted_in_navigation_ = false;

  // We keep track of when the user began modifying the searchbox text.
  // This should be valid whenever user_input_in_progress_ is true.
  base::TimeTicks time_user_first_modified_omnibox_;

  // We keep track of when the user selected a match.
  base::TimeTicks match_selection_timestamp_;
};

// Handles the acceptance of a match from a WebUI searchbox.
// Generates a URL_WHAT_YOU_TYPED match with ".com" appended.
// If |generated_input| is provided, it will be updated with the new input used
// to generate the match.
AutocompleteMatch GenerateDotComMatch(
    OmniboxClient* client,
    AutocompleteController* autocomplete_controller,
    const AutocompleteInput& original_input,
    const std::u16string& text_for_desired_tld_navigation,
    AutocompleteInput* generated_input = nullptr);

// Handles opening a match (called by AcceptInput).
void OpenMatch(AutocompleteController* autocomplete_controller,
               OmniboxClient* client,
               const AutocompleteInput& input,
               OmniboxPopupSelection selection,
               AutocompleteMatch match,
               WindowOpenDisposition disposition,
               const InteractionMetricsTracker& metrics_tracker,
               metrics::OmniboxEventProto::KeywordModeEntryMethod
                   keyword_mode_entry_method,
               const std::u16string& pasted_text);

// Determines whether the user can "paste and go", given the specified text.
bool CanPasteAndGo(OmniboxClient* client, const std::u16string& text);

// Navigates to the destination for given "paste and go" text.
void PasteAndGo(AutocompleteController* autocomplete_controller,
                OmniboxClient* client,
                const std::u16string& text,
                const InteractionMetricsTracker& metrics_tracker =
                    InteractionMetricsTracker(),
                metrics::OmniboxEventProto::KeywordModeEntryMethod
                    keyword_mode_entry_method =
                        metrics::OmniboxEventProto::INVALID);

// Utility functions to preserve histogram parity with OmniboxEditModel.
void RecordNonActionSearchMetrics(TemplateURLService* template_url_service,
                                  const AutocompleteMatch& match,
                                  bool is_off_the_record,
                                  base::TimeTicks match_selection_timestamp);
void EmitAcceptedKeywordSuggestionHistogram(
    metrics::OmniboxEventProto::KeywordModeEntryMethod entry_method,
    const TemplateURL* turl);
void RecordSuggestionUsedMetrics(const AutocompleteMatch& match);

WindowOpenDisposition ComputeOpenDispositionFromModifiersAndLogToUma(
    bool shift,
    bool control,
    bool alt,
    bool command);

}  // namespace searchbox

#endif  // COMPONENTS_OMNIBOX_BROWSER_SEARCHBOX_UTILS_H_
