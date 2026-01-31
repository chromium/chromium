// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import org.chromium.base.UserData;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.omnibox.AutocompleteInput;

/**
 * Fusebox / Omnibox session state object. Captures controllers and state details needed to fulfill
 * or reconstruct the user input.
 *
 * <p>Unlike the AutocompleteInput - this class is permitted to hold external controllers required
 * to fulfill navigation request.
 */
@NullMarked
public class FuseboxSessionState implements UserData {
    /**
     * Details about the user input in the Omnibox. Retained to allow session reconstruction, for
     * example when the user switches tabs.
     */
    public final AutocompleteInput autocompleteInput = new AutocompleteInput();

    private boolean mIsActive;

    /**
     * Retrieve the session state for the supplied Tab.
     *
     * @param tab The tab to retrieve the session state for.
     * @return FuseboxSessionState for the supplied tab, or null if the tab is not valid.
     */
    static @Nullable FuseboxSessionState from(@Nullable Tab tab) {
        if (tab == null || tab.isDestroyed()) {
            return null;
        }
        FuseboxSessionState state = tab.getUserDataHost().getUserData(FuseboxSessionState.class);
        if (state == null) {
            state = new FuseboxSessionState();
            tab.getUserDataHost().setUserData(FuseboxSessionState.class, state);
        }
        return state;
    }

    /**
     * Marks the session as active or inactive. When session is marked as inactive, the autocomplete
     * input is reset.
     *
     * @param isActive Whether the session should be active.
     */
    public void setSessionActive(boolean isActive) {
        mIsActive = isActive;
        if (!isActive) {
            autocompleteInput.reset();
        }
    }

    /**
     * @return Whether the Fusebox session is active.
     */
    public boolean isSessionActive() {
        return mIsActive;
    }
}
