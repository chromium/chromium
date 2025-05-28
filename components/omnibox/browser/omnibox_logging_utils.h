// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_LOGGING_UTILS_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_LOGGING_UTILS_H_

#include "base/time/time.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

struct AutocompleteMatch;
class AutocompleteResult;

namespace omnibox {

// Logs metrics related to the time elapsed from when the user focused the
// omnibox until a suggestion is opened/acted upon.
void LogFocusToOpenTime(
    base::TimeDelta elapsed,
    bool is_zero_prefix,
    ::metrics::OmniboxEventProto::PageClassification page_classification,
    const AutocompleteMatch& match,
    size_t action_index);

// `executed_selection` indicates which OmniboxAction within `result`
// was executed, and leaving this parameter as the default indicates
// that no action was executed.
void RecordActionShownForAllActions(
    const AutocompleteResult& result,
    OmniboxPopupSelection executed_selection =
        OmniboxPopupSelection(OmniboxPopupSelection::kNoMatch));

}  // namespace omnibox

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_LOGGING_UTILS_H_
