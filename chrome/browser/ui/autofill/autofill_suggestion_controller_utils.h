// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_SUGGESTION_CONTROLLER_UTILS_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_SUGGESTION_CONTROLLER_UTILS_H_

#include <vector>

#include "base/containers/span.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"

namespace content {
class BrowserContext;
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace autofill {

class AutofillPopupDelegate;
struct SelectOption;
struct Suggestion;

// Returns whether this `PopupItemId` can, in principle, be accepted. Note that
// even if this is true, the suggestion itself may still not be acceptable.
bool IsAcceptablePopupItemId(PopupItemId id);

// Returns the RenderFrameHost` corresponding to an `AutofillPopupDelegate`.
content::RenderFrameHost* GetRenderFrameHost(AutofillPopupDelegate& delegate);

// Returns whether `descendendant` is a `descendant` of `ancestor`.
bool IsAncestorOf(content::RenderFrameHost* ancestor,
                  content::RenderFrameHost* descendant);

// Returns whether the pointer is locked in `web_contents`.
bool IsPointerLocked(content::WebContents* web_contents);

// Informs the IPH trackers about an accepted suggestion if the suggestion had
// relevance for IPH.
void NotifyIphAboutAcceptedSuggestion(content::BrowserContext* browser_context,
                                      const Suggestion& suggestion);

std::vector<Suggestion> UpdateSuggestionsFromDataList(
    base::span<const SelectOption> options,
    std::vector<Suggestion> suggestions);

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_SUGGESTION_CONTROLLER_UTILS_H_
