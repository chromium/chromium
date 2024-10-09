// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_SUGGESTION_CONTROLLER_UTILS_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_SUGGESTION_CONTROLLER_UTILS_H_

#include <vector>

#include "base/containers/span.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace autofill {

class AutofillSuggestionDelegate;
struct SelectOption;
struct Suggestion;

// Returns whether this `SuggestionType` can, in principle, be accepted. Note
// that even if this is true, the suggestion itself may still not be acceptable.
bool IsAcceptableSuggestionType(SuggestionType id);

// Returns whether the suggestion with this `type` belongs into
// the footer section of the popup. Returns `false` for separators, which may
// belong either to the main or the footer section.
bool IsFooterSuggestionType(SuggestionType type);

// Returns `true` if the item at `line_number` belongs into the footer section
// of the popup. For separators, the result is that of the next item.
bool IsFooterItem(const std::vector<Suggestion>& suggestions,
                  size_t line_number);

// Returns `true` if the popup should remain open with a suggestion of `type`
// as the first suggestion (e.g. after deleting a suggestion). This is true for
// all non-footer suggestions and false for most footer suggestions.
bool IsStandaloneSuggestionType(SuggestionType type);

// Returns the RenderFrameHost` corresponding to an
// `AutofillSuggestionDelegate`.
content::RenderFrameHost* GetRenderFrameHost(
    AutofillSuggestionDelegate& delegate);

// Returns whether `descendendant` is a `descendant` of `ancestor`.
bool IsAncestorOf(content::RenderFrameHost* ancestor,
                  content::RenderFrameHost* descendant);

// Returns whether the pointer is locked in `web_contents`.
bool IsPointerLocked(content::WebContents* web_contents);

// Informs the user education trackers about an accepted suggestion if the
// suggestion had relevance for in-product-help or for "new" badges.
void NotifyUserEducationAboutAcceptedSuggestion(content::WebContents* contents,
                                                const Suggestion& suggestion);

std::vector<Suggestion> UpdateSuggestionsFromDataList(
    base::span<const SelectOption> options,
    std::vector<Suggestion> suggestions);

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_SUGGESTION_CONTROLLER_UTILS_H_
