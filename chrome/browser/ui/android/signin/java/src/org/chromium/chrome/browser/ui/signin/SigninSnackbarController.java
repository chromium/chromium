// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import androidx.activity.ComponentActivity;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncHelper;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SignoutReason;

/**
 * Shows a snackbar after a successful sign-in, which allows the user to undo the sign-in. This
 * snackbar mentions the signed-in user's email and offers an "Undo" action button.
 */
@NullMarked
class SigninSnackbarController implements SnackbarManager.SnackbarController {

    private static final String TAG = "SigninSnackbar";

    /**
     * Listener interface used to notify the embedding component (such as the SigninPromo MVC) of
     * sign-in reversal events.
     */
    interface Listener {
        /**
         * Called after the user clicks the "Undo" action button within the sign-in completion
         * snackbar.
         *
         * <p>The {@code SigninSnackbarController} handles the core reversal logic, ensuring this
         * callback executes after the user has been signed out and history sync has been optionally
         * opted out.
         */
        void onUndoSignin();
    }

    private final ComponentActivity mActivity;
    private final Profile mProfile;
    private final HistorySyncHelper mHistorySyncHelper;
    private final SnackbarManager mSnackbarManager;
    private @Nullable Listener mListener;

    private SigninSnackbarController(
            ComponentActivity activity,
            Profile profile,
            HistorySyncHelper historySyncHelper,
            SnackbarManager snackbarManager,
            Listener listener) {
        mActivity = activity;
        mProfile = profile;
        mHistorySyncHelper = historySyncHelper;
        mSnackbarManager = snackbarManager;
        mListener = listener;
    }

    /**
     * Implements {@link SnackbarController}. actionData contains the exact actions user performed
     * during the sign-in and history sync flow
     */
    @Override
    public void onAction(@Nullable Object actionData) {
        SigninAndHistorySyncCoordinator.Result result =
                (SigninAndHistorySyncCoordinator.Result) assertNonNull(actionData);
        if (result.hasOptedInHistorySync) {
            // We disable history sync only if the user explicitly opted into it during the flow.
            // This ensure we don't interfere with sticky settings from prior sessions.
            mHistorySyncHelper.setHistoryAndTabsSync(false);
        }

        SignOutCoordinator.undoSignInWithSnackbar(
                mActivity.getApplicationContext(),
                mProfile,
                mSnackbarManager,
                SignoutReason.USER_TAPPED_UNDO_RIGHT_AFTER_SIGN_IN,
                () -> {
                    assertNonNull(mListener).onUndoSignin();
                    mListener = null;
                });
    }

    @Override
    public void onDismissNoAction(@Nullable Object actionData) {
        mListener = null;
    }

    /** Shows a snackbar if the sign-in was successful, allowing the user to undo the action. */
    public static void showUndoSnackbarIfNeeded(
            ComponentActivity activity,
            Profile profile,
            @Nullable SnackbarManager snackbarManager,
            Listener listener,
            SigninAndHistorySyncCoordinator.Result result) {
        if (snackbarManager == null) {
            // TODO(crbug.com/437038737): Once SnackbarManager configuration is hardened and always
            // non-null, remove @Nullable and null check.
            Log.e(TAG, "SnackbarManager for displaying sign-in confirmation snackbar is null");
            return;
        }
        if (result.hasSignedIn) {
            // TODO(crbug.com/437039311): No snackbar shown if the sign-in is cancelled by the user
            // with the X button.
            IdentityManager identityManager =
                    assumeNonNull(IdentityServicesProvider.get().getIdentityManager(profile));
            CoreAccountInfo accountInfo =
                    identityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN);
            if (accountInfo == null) {
                return;
            }
            String email = accountInfo.getEmail();
            Snackbar snackbar =
                    Snackbar.make(
                            activity.getString(R.string.snackbar_signed_in_as, email),
                            new SigninSnackbarController(
                                    activity,
                                    profile,
                                    HistorySyncHelper.getForProfile(profile),
                                    snackbarManager,
                                    assertNonNull(listener)),
                            Snackbar.TYPE_ACTION,
                            Snackbar.UMA_SIGN_IN);
            snackbar.setAction(activity.getString(R.string.snackbar_undo_signin), result);
            snackbarManager.showSnackbar(snackbar);
        }
    }
}
