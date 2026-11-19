// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.action;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.components.omnibox.action.ActionPresentationMode;
import org.chromium.components.omnibox.action.OmniboxAction;
import org.chromium.components.omnibox.action.OmniboxActionDelegate;
import org.chromium.components.omnibox.action.OmniboxActionId;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.url.GURL;

/**
 * Omnibox takeover action for contextual (zero-prefix) search suggestions. Corresponds to
 * ContextualSearchFulfillmentAction in C++. Executing it asks the {@link OmniboxActionDelegate} to
 * fulfill the suggestion's query with the current tab attached as context.
 */
@NullMarked
public class OmniboxContextualSearchFulfillmentAction extends OmniboxAction {
    /** The search URL of the associated suggestion, including its search terms args. */
    public final GURL fulfillmentUrl;

    public OmniboxContextualSearchFulfillmentAction(
            long nativeInstance, String hint, String accessibilityHint, GURL fulfillmentUrl) {
        // Use NO_ICON so BasicSuggestionProcessor preserves server-provided suggestion icons
        // (e.g. SEARCH_LOOP_WITH_SPARKLE) while still falling back to the magnifier when unset.
        super(
                OmniboxActionId.CONTEXTUAL_SEARCH_FULFILLMENT,
                nativeInstance,
                hint,
                accessibilityHint,
                NO_ICON,
                R.style.TextAppearance_ChipText,
                ActionPresentationMode.CHIP,
                WindowOpenDisposition.CURRENT_TAB);
        this.fulfillmentUrl = fulfillmentUrl;
    }

    @Override
    public boolean execute(OmniboxActionDelegate delegate) {
        delegate.issueContextualSearchQuery(fulfillmentUrl);
        return true;
    }
}
