// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import org.chromium.base.UserData;
import org.chromium.base.UserDataHost;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteRequestType;

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
     * Retrieve the session state for the supplied Tab, or an ephemeral session state if no tab
     * exists.
     *
     * @param dataProvider The {@link LocationBarDataProvider} to retrieve the current tab from.
     * @return FuseboxSessionState appropriate for the supplied LocationBarDataProvider.
     */
    public static @Nullable FuseboxSessionState from(LocationBarDataProvider dataProvider) {
        var userDataHost = dataProvider.getUserDataHost();
        if (userDataHost == null) return null;

        var state = getSessionForTab(userDataHost);
        // Re-apply page metadata in case of ephemeral session, background reload etc.
        state.autocompleteInput.setPageClassification(dataProvider.getPageClassification(false));
        state.autocompleteInput.setPageUrl(dataProvider.getCurrentGurl());
        state.autocompleteInput.setPageTitle(dataProvider.getTitle());
        return state;
    }

    /**
     * Returns session state for the supplied tab.
     *
     * @param tab The tab to retrieve the session state for.
     * @return FuseboxSessionState for the supplied UserDataHost.
     */
    private static FuseboxSessionState getSessionForTab(UserDataHost userDataHost) {
        FuseboxSessionState state = userDataHost.getUserData(FuseboxSessionState.class);
        if (state == null) {
            state = new FuseboxSessionState();
            userDataHost.setUserData(FuseboxSessionState.class, state);
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
        if (isActive == mIsActive) return;

        mIsActive = isActive;
        if (isActive) {
            autocompleteInput.setRequestType(AutocompleteRequestType.SEARCH);
            autocompleteInput.setUrlFocusTime(System.currentTimeMillis());
        } else {
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
