// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.action;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.omnibox.R;
import org.chromium.components.omnibox.action.OmniboxAction;
import org.chromium.components.omnibox.action.OmniboxActionDelegate;
import org.chromium.components.omnibox.action.OmniboxActionId;
import org.chromium.ui.mojom.WindowOpenDisposition;

/** Omnibox action for Site Search. */
@NullMarked
public class SiteSearchAction extends OmniboxAction {
    /** The keyword for site search. */
    public final String keyword;

    public SiteSearchAction(
            long nativeInstance, String hint, String accessibilityHint, String keyword) {
        super(
                OmniboxActionId.SITE_SEARCH,
                nativeInstance,
                hint,
                accessibilityHint,
                // TODO(crbug.com/459590224): Change it to search icon.
                DEFAULT_ICON,
                R.style.TextAppearance_ChipText,
                /* showAsActionButton= */ false,
                WindowOpenDisposition.CURRENT_TAB);
        this.keyword = keyword;
    }

    @Override
    public boolean execute(OmniboxActionDelegate delegate) {
        if (delegate.getAutocompleteInput() != null) {
            delegate.getAutocompleteInput().setKeyword(keyword);
        }
        return false; // do not clear omnibox focus.
    }
}
