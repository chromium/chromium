// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_SEARCHBOX_UTILS_H_
#define COMPONENTS_OMNIBOX_BROWSER_SEARCHBOX_UTILS_H_

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
               OmniboxPopupSelection selection,
               AutocompleteMatch match,
               WindowOpenDisposition disposition,
               base::TimeTicks searchbox_focused_timestamp,
               base::TimeTicks first_modification_timestamp,
               base::TimeTicks match_selection_timestamp,
               metrics::OmniboxEventProto::KeywordModeEntryMethod
                   keyword_mode_entry_method);

// Utility functions to preserve histogram parity with OmniboxEditModel.
void RecordNonActionSearchMetrics(TemplateURLService* template_url_service,
                                  const AutocompleteMatch& match,
                                  bool is_off_the_record,
                                  base::TimeTicks match_selection_timestamp);
void EmitAcceptedKeywordSuggestionHistogram(
    metrics::OmniboxEventProto::KeywordModeEntryMethod entry_method,
    const TemplateURL* turl);
void RecordSuggestionUsedMetrics(const AutocompleteMatch& match);

}  // namespace searchbox

#endif  // COMPONENTS_OMNIBOX_BROWSER_SEARCHBOX_UTILS_H_
