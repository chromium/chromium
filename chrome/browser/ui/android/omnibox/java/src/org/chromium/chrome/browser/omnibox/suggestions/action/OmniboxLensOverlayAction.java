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

/**
 * Omnibox action for opening the Lens Overlay / Contextual Search. Corresponds to
 * ContextualSearchOpenLensAction in C++.
 */
@NullMarked
public class OmniboxLensOverlayAction extends OmniboxAction {
    public OmniboxLensOverlayAction(long nativeInstance, String hint, String accessibilityHint) {
        super(
                OmniboxActionId.CONTEXTUAL_SEARCH_OPEN_LENS,
                nativeInstance,
                hint,
                accessibilityHint,
                new ActionIcon(R.drawable.ic_suggestion_magnifier, /* tintWithTextColor= */ true),
                R.style.TextAppearance_ChipText,
                ActionPresentationMode.CHIP,
                WindowOpenDisposition.CURRENT_TAB);
    }

    @Override
    public boolean execute(OmniboxActionDelegate delegate) {
        delegate.openLensOverlay();
        return true;
    }
}
