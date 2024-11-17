// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_log.h"

#include "components/omnibox/browser/autocomplete_result.h"

OmniboxLog::OmniboxLog(
    const std::u16string& text,
    bool just_deleted_text,
    metrics::OmniboxInputType input_type,
    bool in_keyword_mode,
    metrics::OmniboxEventProto::KeywordModeEntryMethod entry_method,
    bool is_popup_open,
    OmniboxPopupSelection selection,
    WindowOpenDisposition disposition,
    bool is_paste_and_go,
    SessionID tab_id,
    metrics::OmniboxEventProto::PageClassification current_page_classification,
    base::TimeDelta elapsed_time_since_user_first_modified_omnibox,
    size_t completed_length,
    base::TimeDelta elapsed_time_since_last_change_to_default_match,
    const AutocompleteResult& result,
    const GURL& final_destination_url,
    bool is_incognito)
    : text(text),
      just_deleted_text(just_deleted_text),
      input_type(input_type),
      in_keyword_mode(in_keyword_mode),
      keyword_mode_entry_method(entry_method),
      is_popup_open(is_popup_open),
      selection(selection),
      disposition(disposition),
      is_paste_and_go(is_paste_and_go),
      tab_id(tab_id),
      current_page_classification(current_page_classification),
      elapsed_time_since_user_first_modified_omnibox(
          elapsed_time_since_user_first_modified_omnibox),
      completed_length(completed_length),
      elapsed_time_since_last_change_to_default_match(
          elapsed_time_since_last_change_to_default_match),
      result(result),
      final_destination_url(final_destination_url),
      is_incognito(is_incognito),
      steady_state_omnibox_position(
          metrics::OmniboxEventProto::UNKNOWN_POSITION),
      ukm_source_id(ukm::kInvalidSourceId) {
  DCHECK(selection.line < result.size())
      << "The selection line index should always be valid. See comments on "
         "OmniboxLog::selection.";
}

OmniboxLog::~OmniboxLog() = default;
