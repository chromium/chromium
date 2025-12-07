// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncHelper;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler.BackPressResult;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;

/**
 * Interface for coordinators responsible of showing the correct sub-component of the sign-in and
 * history opt-in flow.
 */
@NullMarked
public interface SigninAndHistorySyncCoordinator {

    /** Indicates the sign-in flow completion status. */
    public class Result {
        /**
         * Whether the sign-in operation occurred during this specific execution of the flow. Should
         * be False if the user was already signed in before the flow started. Note, if the user
         * explicitly accepts the sign-in CTA and the history sync consent is required, then
         * declining history sync invalidates the entire sign-in.
         */
        public final boolean hasSignedIn;

        /**
         * The user successfully completed the history sync enablement step during the flow. Note,
         * it possible for an already signed-in user to not be shown the sign-in CTA and only the
         * history sync consent dialog.
         */
        public final boolean hasOptedInHistorySync;

        public Result(boolean hasSignedIn, boolean hasOptedInHistorySync) {
            this.hasSignedIn = hasSignedIn;
            this.hasOptedInHistorySync = hasOptedInHistorySync;
        }

        /** Default non-completion state, user canceled the sign-in flow, or an error occurred. */
        public static Result aborted() {
            return new Result(false, false);
        }
    }

    /** Cleans up the coordinator after it is finished being used. */
    void destroy();

    /**
     * Called when an Google Play Services "add account" flow started at the activity level has
     * finished without being completed.
     */
    void onAddAccountCanceled();

    /**
     * Called when an account is added via Google Play Services "add account" flow started at the
     * activity level.
     */
    void onAccountAdded(String accountEmail);

    /** Provides the root view of the sign-in and history opt-in flow. */
    View getView();

    /** Called when the configuration of the embedder activity changes. */
    void onConfigurationChange();

    /** Called when a backpress occurs in the embedder activity. */
    @BackPressResult
    int handleBackPress();

    /**
     * Whether the sign-in ui will show in the sign-in flow if the latter is launched.
     *
     * <p>The sign-in UI can be skipped if the user is already signed-in, for instance.
     *
     * @param profile The current profile.
     */
    static boolean willShowSigninUi(Profile profile) {
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(profile);
        assumeNonNull(signinManager);
        return signinManager.isSigninAllowed();
    }

    /**
     * Whether the history sync opt-in ui will show in the sign-in flow if the latter is launched.
     *
     * <p>The history sync opt-in can be skipped if the user is already opted-in, or if sync or the
     * history sync data types are managed, for instance.
     *
     * @param profile The current profile.
     * @param historyOptInMode Whether the history opt-in should be always, optionally or never
     *     shown.
     */
    static boolean willShowHistorySyncUi(
            Profile profile, @HistorySyncConfig.OptInMode int historyOptInMode) {
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(profile);
        assumeNonNull(identityManager);
        if (!willShowSigninUi(profile) && !identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)) {
            // Signin is suppressed because of something other than the user being signed in. Since
            // the user cannot sign in, we should not show history sync either.
            return false;
        }
        return shouldShowHistorySync(profile, historyOptInMode);
    }

    static boolean shouldShowHistorySync(
            Profile profile, @HistorySyncConfig.OptInMode int historyOptInMode) {
        HistorySyncHelper historySyncHelper = HistorySyncHelper.getForProfile(profile);
        boolean forceHistoryOptInScreen =
                SigninFeatureMap.isEnabled(SigninFeatures.FORCE_HISTORY_OPT_IN_SCREEN);
        return switch (historyOptInMode) {
            case HistorySyncConfig.OptInMode.NONE -> false;
            case HistorySyncConfig.OptInMode.OPTIONAL ->
                    historySyncHelper.shouldDisplayHistorySync()
                            && (forceHistoryOptInScreen || !historySyncHelper.isDeclinedOften());
            case HistorySyncConfig.OptInMode.REQUIRED ->
                    historySyncHelper.shouldDisplayHistorySync();
            default ->
                    throw new IllegalArgumentException(
                            "Unexpected value for historyOptInMode :" + historyOptInMode);
        };
    }
}
