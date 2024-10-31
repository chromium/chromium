// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncHelper;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler.BackPressResult;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Interface for coordinators responsible of showing the correct sub-component of the sign-in and
 * history opt-in flow.
 */
public interface SigninAndHistorySyncCoordinator {

    /** Indicates the sign-in flow completion status. */
    @IntDef({
        Result.COMPLETED,
        Result.INTERRUPTED,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface Result {
        /** Indicates the sign-in flow completed successfully. */
        int COMPLETED = 0;

        /**
         * Indicates the sign-in flow was not completed due to error. The conditions depend on the
         * configuration of the sign-in flow: e.g. if history opt-in is shown, declining history
         * opt-in will set the INTERRUPTED state, and same for the sign-in step.
         */
        int INTERRUPTED = 1;
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
    void onAccountAdded(@NonNull String accountEmail);

    /** Provides the root view of the sign-in and history opt-in flow. */
    @NonNull
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
    public static boolean willShowSigninUi(Profile profile) {
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(profile);
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
    public static boolean willShowHistorySyncUi(
            Profile profile, @HistorySyncConfig.OptInMode int historyOptInMode) {
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(profile);
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
        return switch (historyOptInMode) {
            case HistorySyncConfig.OptInMode.NONE -> false;
            case HistorySyncConfig.OptInMode.OPTIONAL -> !historySyncHelper
                            .shouldSuppressHistorySync()
                    && !historySyncHelper.isDeclinedOften();
            case HistorySyncConfig.OptInMode.REQUIRED -> !historySyncHelper
                    .shouldSuppressHistorySync();
            default -> throw new IllegalArgumentException(
                    "Unexpected value for historyOptInMode :" + historyOptInMode);
        };
    }
}
