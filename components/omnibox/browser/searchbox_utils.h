// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_SEARCHBOX_UTILS_H_
#define COMPONENTS_OMNIBOX_BROWSER_SEARCHBOX_UTILS_H_

#include "base/time/time.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
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
               base::TimeTicks match_selection_timestamp);

// Utility function to preserve histogram parity with OmniboxEditModel.
void RecordDefaultSearchProviderSearchMetrics(bool is_off_the_record);

}  // namespace searchbox

#endif  // COMPONENTS_OMNIBOX_BROWSER_SEARCHBOX_UTILS_H_
