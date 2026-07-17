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

// Handles the acceptance of a match from a WebUI searchbox.
void OpenMatch(AutocompleteController* autocomplete_controller,
               OmniboxClient* client,
               const AutocompleteInput& input,
               OmniboxPopupSelection selection,
               AutocompleteMatch match,
               WindowOpenDisposition disposition,
               base::TimeTicks searchbox_focused_timestamp,
               base::TimeTicks first_modification_timestamp,
               base::TimeTicks match_selection_timestamp,
               metrics::OmniboxEventProto::KeywordModeEntryMethod
                   keyword_mode_entry_method,
               const std::u16string& pasted_text);

// Determines whether the user can "paste and go", given the specified text.
bool CanPasteAndGo(OmniboxClient* client, const std::u16string& text);

// Navigates to the destination for given "paste and go" text.
void PasteAndGo(
    AutocompleteController* autocomplete_controller,
    OmniboxClient* client,
    const std::u16string& text,
    base::TimeTicks searchbox_focused_timestamp = base::TimeTicks(),
    base::TimeTicks first_modification_timestamp = base::TimeTicks(),
    base::TimeTicks match_selection_timestamp = base::TimeTicks(),
    metrics::OmniboxEventProto::KeywordModeEntryMethod
        keyword_mode_entry_method = metrics::OmniboxEventProto::INVALID);

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
