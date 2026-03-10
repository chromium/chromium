// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.UserData;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.OneShotCallback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.fusebox.ComposeboxQueryControllerBridge;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxAttachmentModelList;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxAttachmentModelList.FuseboxAttachmentChangeListener;
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
    private final FuseboxAttachmentChangeListener mFuseboxAttachmentChangeListener =
            new FuseboxAttachmentChangeListener() {
                @Override
                public void onAttachmentListChanged() {
                    FuseboxSessionState.this.onAttachmentListChanged();
                }
            };
    private final Callback<Integer> mOnToolModeChanged = this::onToolModeChanged;

    /**
     * Details about the user input in the Omnibox. Retained to allow session reconstruction, for
     * example when the user switches tabs.
     */
    private final AutocompleteInput mAutocompleteInput;

    private @Nullable Profile mProfile;
    private @Nullable ComposeboxQueryControllerBridge mComposeBoxQueryControllerBridge;
    private @Nullable FuseboxAttachmentModelList mFuseboxAttachmentModelList;
    private @Nullable OneShotCallback<Profile> mPendingProfileCallback;
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
     * @param userDataHost The tab to retrieve the session state for.
     * @return FuseboxSessionState for the supplied UserDataHost.
     */
    private static FuseboxSessionState getSessionForTab(UserDataHost userDataHost) {
        FuseboxSessionState state = userDataHost.getUserData(FuseboxSessionState.class);
        if (state == null) {
            state = new FuseboxSessionState(new AutocompleteInput());
            userDataHost.setUserData(FuseboxSessionState.class, state);
        }
        return state;
    }

    /** Constructs a new, empty FuseboxSessionState. */
    private FuseboxSessionState(AutocompleteInput input) {
        mAutocompleteInput = input;
        mAutocompleteInput.getToolModeSupplier().addSyncObserver(mOnToolModeChanged);
    }

    /** A test only constructor with initial values. */
    @VisibleForTesting
    public FuseboxSessionState(
            AutocompleteInput input,
            @Nullable ComposeboxQueryControllerBridge composeboxQueryControllerBridge,
            @Nullable FuseboxAttachmentModelList fuseboxAttachmentModelList) {
        this(input);
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
     * Marks the session as active.
     *
     * <p>When a session is marked as active, it will asynchronously acquire a {@link Profile} and
     * initialize all required session controllers. The caller may supply an optional {@link
     * Runnable} to be notified when the session is fully set up.
     *
     * @param profileSupplier The supplier for the {@link Profile} object.
     * @param onFullyActivated Optional runnable to be invoked when the session is fully activated.
     */
    public void activate(
            MonotonicObservableSupplier<Profile> profileSupplier,
            @Nullable Runnable onFullyActivated) {
        if (mIsActive) {
            // This session is being re-activated. It has already been fully initialized so simply
            // emit the event.
            if (onFullyActivated != null) onFullyActivated.run();
            return;
        }

        mIsActive = true;
        mAutocompleteInput.setUrlFocusTime(System.currentTimeMillis());
        // Use current URL if the Retention is active, the input is not already set, and the URL
        // should be user-visible.
        if (OmniboxFeatures.shouldRetainOmniboxOnFocus()
                && mAutocompleteInput.getUserText().isEmpty()
                && UrlBarData.shouldShowUrl(mAutocompleteInput.getPageUrl(), false)) {
            var editUrl = UrlUtilities.stripScheme(mAutocompleteInput.getPageUrl().getSpec());
            mAutocompleteInput.setUserText(editUrl).setSelection(0, Integer.MAX_VALUE);
        }

        // Stop here if we're already waiting for profile.
        // This makes sense in scenarios where session object goes through a full cycle
        // (active -> inactive -> active again) before Profile becomes available, to avoid
        // requesting multiple session controllers.
        if (mPendingProfileCallback != null) return;

        mPendingProfileCallback =
                new OneShotCallback<>(
                        profileSupplier, p -> setUpSessionControllers(p, onFullyActivated));
    }

    /**
     * Marks the session as inactive.
     *
     * <p>When session is marked as inactive, the autocomplete input is reset.
     */
    public void deactivate() {
        if (!mIsActive) return;

        mAutocompleteInput.reset();
        tearDownSessionControllers();
        mIsActive = false;
    }

    /**
     * Set up session controllers for the supplied profile.
     *
     * @param profile The profile the session is activated for.
     * @param onFullyActivated Optional runnable to be invoked when the session is fully activated.
     */
    private void setUpSessionControllers(Profile profile, @Nullable Runnable onFullyActivated) {
        // Record the event that we're not waiting for profile anymore.
        mPendingProfileCallback = null;

        // If the session became inactive while we wait for the profile - don't accept the new
        // profile.
        if (!mIsActive) return;

        // The only valid transition is no profile -> profile. Must not create duplicate
        // controllers.
        assert (mProfile == null);
        mProfile = profile;

        mComposeBoxQueryControllerBridge =
                ComposeboxQueryControllerBridge.createForProfile(mProfile);

        if (mComposeBoxQueryControllerBridge != null) {
            // Composebox Controller may not be instantiated if locale or policies prohibit AIM.
            // Create attachments list only if allowed.
            mFuseboxAttachmentModelList = new FuseboxAttachmentModelList();
            mFuseboxAttachmentModelList.setComposeboxQueryControllerBridge(
                    mComposeBoxQueryControllerBridge);
            mFuseboxAttachmentModelList.addAttachmentChangeListener(
                    mFuseboxAttachmentChangeListener);
        }

        if (onFullyActivated != null) onFullyActivated.run();
    }

    /** Tear down session controllers. */
    private void tearDownSessionControllers() {
        if (mFuseboxAttachmentModelList != null) {
            mFuseboxAttachmentModelList.removeAttachmentChangeListener(
                    mFuseboxAttachmentChangeListener);
            mFuseboxAttachmentModelList.destroy();
            mFuseboxAttachmentModelList = null;
        }

        if (mComposeBoxQueryControllerBridge != null) {
            mComposeBoxQueryControllerBridge.destroy();
            mComposeBoxQueryControllerBridge = null;
        }

        mProfile = null;
    }

    private void onAttachmentListChanged() {
        // TODO(https://crbug.com/474616308): Check if possible to combine input and attachments as
        // FuseboxInput and remove.
        boolean hasAttachments =
                mFuseboxAttachmentModelList != null && !mFuseboxAttachmentModelList.isEmpty();
        mAutocompleteInput.setHasAttachments(hasAttachments);
    }

    private void onToolModeChanged(int toolMode) {
        if (mComposeBoxQueryControllerBridge != null) {
            mComposeBoxQueryControllerBridge.setActiveTool(toolMode);
        }
    }

    /** Returns whether the Fusebox session is active. */
    public boolean isSessionActive() {
        return mIsActive;
    }

    /** Modifies this session input to have the values of the given input. */
    public void applyAutocompleteInput(AutocompleteInput input) {
        mAutocompleteInput.copyFrom(input);
    }

    /** Returns the current {@link AutocompleteInput} for this session. */
    public AutocompleteInput getAutocompleteInput() {
        return mAutocompleteInput;
    }

    /** Returns the current {@link ComposeboxQueryControllerBridge} for this session. */
    public @Nullable ComposeboxQueryControllerBridge getComposeboxQueryControllerBridge() {
        return mComposeBoxQueryControllerBridge;
    }

    /** Returns the current {@link FuseboxAttachmentModelList} for this session. */
    public @Nullable FuseboxAttachmentModelList getFuseboxAttachmentModelList() {
        return mFuseboxAttachmentModelList;
    }
}
