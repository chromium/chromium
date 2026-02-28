// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.UserData;
import org.chromium.base.UserDataHost;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.fusebox.ComposeboxQueryControllerBridge;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxAttachmentModelList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.OmniboxFeatures;

/**
 * Fusebox / Omnibox session state object. Captures controllers and state details needed to fulfill
 * or reconstruct the user input. This object is associated with a specific {@link Profile}.
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
    private AutocompleteInput mAutocompleteInput = new AutocompleteInput();

    private @Nullable Profile mProfile;
    private @Nullable ComposeboxQueryControllerBridge mComposeBoxQueryControllerBridge;
    private @Nullable FuseboxAttachmentModelList mFuseboxAttachmentModelList;
    private boolean mIsActive;

    /**
     * Retrieve the session state for the supplied Tab, or an ephemeral session state if no tab
     * exists.
     *
     * @param dataProvider The {@link LocationBarDataProvider} to retrieve the current tab from.
     * @return FuseboxSessionState appropriate for the supplied LocationBarDataProvider, or `null`
     *     if the UserData to host the persisted session state is not available.
     */
    public static @Nullable FuseboxSessionState from(LocationBarDataProvider dataProvider) {
        var userDataHost = dataProvider.getUserDataHost();
        if (userDataHost == null) return null;

        var state = getSessionForTab(userDataHost);
        // Re-apply page metadata in case of ephemeral session, background reload etc.
        state.mAutocompleteInput.setPageClassification(dataProvider.getPageClassification(false));
        state.mAutocompleteInput.setPageUrl(dataProvider.getCurrentGurl());
        state.mAutocompleteInput.setPageTitle(dataProvider.getTitle());
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

    /** Constructs a new, empty FuseboxSessionState. */
    private FuseboxSessionState() {}

    /**
     * Constructs a new FuseboxSessionState with a provided AutocompleteInput.
     *
     * @param input The initial AutocompleteInput for this session.
     */
    @VisibleForTesting
    public FuseboxSessionState(
            AutocompleteInput input,
            @Nullable ComposeboxQueryControllerBridge composeboxQueryControllerBridge,
            @Nullable FuseboxAttachmentModelList fuseboxAttachmentModelList) {
        mAutocompleteInput = input;
        mComposeBoxQueryControllerBridge = composeboxQueryControllerBridge;
        mFuseboxAttachmentModelList = fuseboxAttachmentModelList;
    }

    /**
     * @return The current {@link Profile} for this session.
     */
    public @Nullable Profile getProfile() {
        return mProfile;
    }

    /**
     * Marks the session as active or inactive. When session is marked as inactive, the autocomplete
     * input is reset.
     *
     * @param isActive Whether the session should be active.
     */
    public void setSessionActive(boolean isActive) {
        if (mIsActive == isActive) return;
        mIsActive = isActive;
        if (isActive) {
            mAutocompleteInput.setUrlFocusTime(System.currentTimeMillis());
            // Use current URL if the Retention is active, the input is not already set, and the URL
            // should be user-visible.
            if (OmniboxFeatures.shouldRetainOmniboxOnFocus()
                    && mAutocompleteInput.getUserText().isEmpty()
                    && UrlBarData.shouldShowUrl(mAutocompleteInput.getPageUrl(), false)) {
                var editUrl = UrlUtilities.stripScheme(mAutocompleteInput.getPageUrl().getSpec());
                mAutocompleteInput.setUserText(editUrl).setSelection(0, Integer.MAX_VALUE);
            }
        } else {
            mAutocompleteInput.reset();
            setProfile(null);
        }
    }

    /**
     * Apply or reset profile to be used with the current Fusebox session.
     *
     * @param profile The profile the session is activated for; must be supplied to activate the
     *     session.
     */
    public void setProfile(@Nullable Profile profile) {
        if (mProfile == profile) return;

        // Profile has changed. This typically means either
        // - profile was applied and we want to construct session objects, or
        // - profile was removed and we want to destroy them.
        // Technically this also supports profile swap, but that scenario shouldn't ever happen.
        mProfile = profile;

        if (mFuseboxAttachmentModelList != null) {
            mFuseboxAttachmentModelList.destroy();
            mFuseboxAttachmentModelList = null;
        }

        if (mComposeBoxQueryControllerBridge != null) {
            mComposeBoxQueryControllerBridge.destroy();
            mComposeBoxQueryControllerBridge = null;
        }

        // Abort now if we're not creating session controllers.
        if (mProfile == null || !mIsActive) return;

        mComposeBoxQueryControllerBridge =
                ComposeboxQueryControllerBridge.createForProfile(mProfile);

        if (mComposeBoxQueryControllerBridge != null) {
            // Composebox Controller may not be instantiated if locale or policies prohibit AIM.
            // Create attachments list only if allowed.
            mFuseboxAttachmentModelList = new FuseboxAttachmentModelList();
            mFuseboxAttachmentModelList.setComposeboxQueryControllerBridge(
                    mComposeBoxQueryControllerBridge);
        }
    }

    /**
     * @return Whether the Fusebox session is active.
     */
    public boolean isSessionActive() {
        return mIsActive;
    }

    /** Applies the new AutocompleteInput to the current Session object. */
    public void setAutocompleteInput(AutocompleteInput newInput) {
        mAutocompleteInput = newInput;
    }

    /**
     * @return The current {@link AutocompleteInput} for this session.
     */
    public AutocompleteInput getAutocompleteInput() {
        return mAutocompleteInput;
    }

    /**
     * @return The current {@link ComposeboxQueryControllerBridge} for this session.
     */
    public @Nullable ComposeboxQueryControllerBridge getComposeboxQueryControllerBridge() {
        return mComposeBoxQueryControllerBridge;
    }

    /**
     * @return The current {@link FuseboxAttachmentModelList} for this session.
     */
    public @Nullable FuseboxAttachmentModelList getFuseboxAttachmentModelList() {
        return mFuseboxAttachmentModelList;
    }
}
