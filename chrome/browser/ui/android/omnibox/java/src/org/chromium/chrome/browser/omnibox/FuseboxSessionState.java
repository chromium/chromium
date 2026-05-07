// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.UserData;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.OneShotCallback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.fusebox.ComposeboxQueryControllerBridge;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxAttachmentModelList;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxAttachmentModelList.FuseboxAttachmentChangeListener;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMetrics;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.ToolModeUtils;
import org.chromium.content_public.browser.WebContents;

import java.util.Optional;

/**
 * Fusebox / Omnibox session state object. Captures controllers and state details needed to fulfill
 * or reconstruct the user input. This object is associated with a specific {@link Profile}.
 *
 * <p>Unlike the AutocompleteInput - this class is permitted to hold external controllers required
 * to fulfill navigation request.
 *
 * <ul>
 *   <li>All FuseboxSessionState members should be considered `final` from the moment the session is
 *       fully activated to the moment the session is deactivated.
 *   <li>Consumers of FuseboxSessionState can cache individual fields of the FuseboxSessionState
 *       between beginInput() and endInput() for easier access to relevant controllers and data.
 *   <li>It is illegal to change reported instances or values once `onFullyActivated` callback has
 *       been emitted, except if the session is being deactivated.
 *   <li>If instances need to be changed for any reason, the caller must call endSession() and
 *       deactivate() first. Once updates are applied, the caller must activate()
 *       FuseboxSessionState and call beginInput() on appropriate consumers.
 * </ul>
 */
@NullMarked
public class FuseboxSessionState implements UserData {
    @SuppressWarnings("NullableOptional")
    public static @Nullable Optional<FuseboxSessionState> sInstanceForTesting;

    private final FuseboxAttachmentChangeListener mFuseboxAttachmentChangeListener =
            new FuseboxAttachmentChangeListener() {
                @Override
                public void onAttachmentListChanged() {
                    FuseboxSessionState.this.onAttachmentListChanged();
                }
            };
    private final Callback<Integer> mOnRequestTypeChanged = this::onRequestTypeChanged;

    /**
     * Details about the user input in the Omnibox. Retained to allow session reconstruction, for
     * example when the user switches tabs.
     */
    private final AutocompleteInput mAutocompleteInput = new AutocompleteInput();

    private @Nullable FuseboxMetrics mMetrics;
    protected @Nullable Profile mProfile;
    private @Nullable ComposeboxQueryControllerBridge mComposeBoxQueryControllerBridge;
    protected @Nullable AutocompleteController mAutocomplete;
    private @Nullable FuseboxAttachmentModelList mFuseboxAttachmentModelList;
    private @Nullable OneShotCallback<Profile> mPendingProfileCallback;
    private @Nullable WebContents mWebContents;
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
        if (sInstanceForTesting != null) return sInstanceForTesting.orElse(null);

        var state = dataProvider.getFuseboxSessionState();
        if (state == null) return null;

        // Re-apply page metadata in case of ephemeral session, background reload etc.
        state.mAutocompleteInput.setPageClassification(dataProvider.getPageClassification(false));
        state.mAutocompleteInput.setPageUrl(dataProvider.getCurrentGurl());
        state.mAutocompleteInput.setPageTitle(dataProvider.getTitle());
        return state;
    }

    /** Constructs a new, empty FuseboxSessionState. */
    public FuseboxSessionState() {
        if (OmniboxFeatures.sShowModelPicker.getValue()) {
            mAutocompleteInput.getRequestTypeSupplier().addSyncObserver(mOnRequestTypeChanged);
        }
    }

    /** Returns the WebContents of the contextual tasks WebUI associated with the fusebox. */
    public @Nullable WebContents getContextualTasksWebContents() {
        return mWebContents;
    }

    /** Returns whether the session is scoped to a specific AI task. */
    public boolean isTaskScoped() {
        return false;
    }

    /** Returns the current {@link Profile} for this session. */
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
     * @param context The context appropriate for the current Activity window.
     * @param webContents The WebContents of the contextual tasks WebUI.
     * @param profileSupplier The supplier for the {@link Profile} object.
     * @param onFullyActivated Optional runnable to be invoked when the session is fully activated.
     */
    public void activate(
            Context context,
            @Nullable WebContents webContents,
            MonotonicObservableSupplier<Profile> profileSupplier,
            @Nullable Runnable onFullyActivated) {
        mWebContents = webContents;
        if (mIsActive) {
            // This session is being re-activated. It has already been fully initialized so simply
            // emit the event.
            linkSessionControllers();
            if (onFullyActivated != null) onFullyActivated.run();
            return;
        }

        mIsActive = true;
        mAutocompleteInput.setUrlFocusTime(System.currentTimeMillis());

        // Use current URL if the Retention is active as the starting input.
        // On eligible LFF devices the Omnibox should, by default, present the
        // current page URL (if the URL is eligible for display).
        if (OmniboxFeatures.hasDesktopExperience(context)
                && UrlBarData.shouldShowUrl(mAutocompleteInput.getPageUrl(), false)) {
            var editUrl = UrlUtilities.stripScheme(mAutocompleteInput.getPageUrl().getSpec());
            mAutocompleteInput.setInitialUserText(editUrl);
        } else {
            mAutocompleteInput.setInitialUserText("");
        }

        // Apply the initial default value unless user text is already set.
        if (mAutocompleteInput.getUserText().isEmpty()) {
            mAutocompleteInput
                    .setUserText(mAutocompleteInput.getInitialUserText())
                    .setSelection(
                            OmniboxFeatures.hasDesktopExperience(context) ? 0 : Integer.MAX_VALUE,
                            Integer.MAX_VALUE);
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
        mWebContents = null;
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

        createAutoComplete(profile);

        if (mComposeBoxQueryControllerBridge == null) {
            mComposeBoxQueryControllerBridge =
                    ComposeboxQueryControllerBridge.create(mProfile, mWebContents, isTaskScoped());
        }

        if (mComposeBoxQueryControllerBridge != null && mFuseboxAttachmentModelList == null) {
            // Composebox Controller may not be instantiated if locale or policies prohibit AIM.
            mMetrics = new FuseboxMetrics();
            // Create attachments list only if allowed.
            mFuseboxAttachmentModelList = new FuseboxAttachmentModelList();
            mFuseboxAttachmentModelList.setComposeboxQueryControllerBridge(
                    mComposeBoxQueryControllerBridge);
            mFuseboxAttachmentModelList.addAttachmentChangeListener(
                    mFuseboxAttachmentChangeListener);
        }

        linkSessionControllers();
        if (onFullyActivated != null) onFullyActivated.run();
    }

    /**
     * Create the AutocompleteController for the session.
     *
     * @param profile The profile to create the controller for.
     */
    protected void createAutoComplete(Profile profile) {
        // AutocompleteController is currently a Profile-keyed instance and does not require
        // explicit destruction.
        mAutocomplete = AutocompleteController.getForProfile(profile);
    }

    @Override
    public void destroy() {
        if (mIsActive) {
            deactivate();
        }
        tearDownSessionControllers();
        if (OmniboxFeatures.sShowModelPicker.getValue()) {
            mAutocompleteInput.getRequestTypeSupplier().removeObserver(mOnRequestTypeChanged);
        }
    }

    /** Unlinks and destroys session controllers. */
    protected void tearDownSessionControllers() {
        unlinkSessionControllers();

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

        mAutocomplete = null;
        mMetrics = null;
        mProfile = null;
    }

    private void linkSessionControllers() {
        if (mAutocomplete == null) return;
        // Write <null> if there's no ComposeBox Bridge (intentional) to ensure decoupled session
        // when user jumps tabs.
        mAutocomplete.setComposeboxQueryControllerBridge(mComposeBoxQueryControllerBridge);
    }

    private void unlinkSessionControllers() {
        if (mAutocomplete == null) return;
        mAutocomplete.setComposeboxQueryControllerBridge(null);
    }

    private void onAttachmentListChanged() {
        // TODO(https://crbug.com/474616308): Check if possible to combine input and attachments as
        // FuseboxInput and remove.
        boolean hasAttachments =
                mFuseboxAttachmentModelList != null && !mFuseboxAttachmentModelList.isEmpty();
        mAutocompleteInput.setHasAttachments(hasAttachments);
    }

    private void onRequestTypeChanged(@AutocompleteRequestType int requestType) {
        assert OmniboxFeatures.sShowModelPicker.getValue();
        if (mComposeBoxQueryControllerBridge != null) {
            int toolMode =
                    ToolModeUtils.getToolModeForRequestType(
                            requestType, /* hasAttachments= */ false);
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

    /** Returns the current {@link FuseboxMetrics} for this session. */
    public @Nullable FuseboxMetrics getMetrics() {
        return mMetrics;
    }

    /** Returns the current {@link ComposeboxQueryControllerBridge} for this session. */
    public @Nullable ComposeboxQueryControllerBridge getComposeboxQueryControllerBridge() {
        return mComposeBoxQueryControllerBridge;
    }

    /** Returns the current {@link AutocompleteController} for this session. */
    public @Nullable AutocompleteController getAutocompleteController() {
        return mAutocomplete;
    }

    /** Returns the current {@link FuseboxAttachmentModelList} for this session. */
    public @Nullable FuseboxAttachmentModelList getFuseboxAttachmentModelList() {
        return mFuseboxAttachmentModelList;
    }

    /**
     * Directly specify FuseboxSessionState object to be used to conduct tests.
     *
     * <p>Avoids creating real objects where mocks are needed.
     */
    public static void setInstanceForTesting(@Nullable FuseboxSessionState state) {
        sInstanceForTesting = Optional.ofNullable(state);
        ResettersForTesting.register(FuseboxSessionState::resetInstanceForTesting);
    }

    /** Revert all overrides for testing. */
    public static void resetInstanceForTesting() {
        sInstanceForTesting = null;
    }
}
