// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.action;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.omnibox.action.ActionPresentationMode;
import org.chromium.components.omnibox.action.OmniboxAction;
import org.chromium.components.omnibox.action.OmniboxActionDelegate;
import org.chromium.components.omnibox.action.OmniboxActionId;
import org.chromium.ui.mojom.WindowOpenDisposition;

/** Omnibox action for opening tabs from other devices. */
@NullMarked
public class CrossDeviceTabAction extends OmniboxAction {
    public CrossDeviceTabAction(long nativeInstance, String hint, String accessibilityHint) {
        super(
                OmniboxActionId.CROSS_DEVICE_TAB,
                nativeInstance,
                hint,
                accessibilityHint,
                new ActionIcon(R.drawable.devices_black_24dp, /* tintWithTextColor= */ true),
                R.style.TextAppearance_ChipText,
                ActionPresentationMode.CHIP,
                WindowOpenDisposition.CURRENT_TAB);
    }

    @Override
    public boolean execute(OmniboxActionDelegate delegate) {
        delegate.loadPageInCurrentTab(UrlConstants.RECENT_TABS_URL);
        return true;
    }
}
