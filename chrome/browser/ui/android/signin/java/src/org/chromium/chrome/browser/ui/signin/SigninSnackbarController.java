// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncHelper;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SignoutReason;

/**
 * Shows a snackbar after a successful sign-in, which allows the user to undo the sign-in. This
 * snackbar mentions the signed-in user's email and offers an "Undo" action button. If the user
 * signs out while the snackbar is shown, the snackbar is dismissed.
 */
@NullMarked
class SigninSnackbarController
        implements SnackbarManager.SnackbarController, IdentityManager.Observer {

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
        void onSigninUndone();
    }

    private final Activity mActivity;
    private final Profile mProfile;
    private final @SignoutReason int mSignoutReason;
    private final HistorySyncHelper mHistorySyncHelper;
    private final SnackbarManager mSnackbarManager;
    private final IdentityManager mIdentityManager;
    private @Nullable Listener mListener;

    private SigninSnackbarController(
            Activity activity,
            Profile profile,
            @SignoutReason int signoutReason,
            HistorySyncHelper historySyncHelper,
            SnackbarManager snackbarManager,
            IdentityManager identityManager,
            Listener listener) {
        mActivity = activity;
        mProfile = profile;
        mSignoutReason = signoutReason;
        mHistorySyncHelper = historySyncHelper;
        mSnackbarManager = snackbarManager;
        mIdentityManager = identityManager;
        mListener = listener;
    }

    /**
     * Implements {@link SnackbarController}. actionData contains the exact actions user performed
     * during the sign-in and history sync flow
     */
    @Override
    public void onAction(@Nullable Object actionData) {
        // Stop observing before signing out, so that the sign-out triggered by the undo action
        // doesn't attempt to dismiss the snackbar again.
        mIdentityManager.removeObserver(this);

        if (!mIdentityManager.hasPrimaryAccount()) {
            mListener = null;
            return;
        }

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
                mSignoutReason,
                () -> {
                    assertNonNull(mListener).onSigninUndone();
                    mListener = null;
                });
    }

    @Override
    public void onDismissNoAction(@Nullable Object actionData) {
        mIdentityManager.removeObserver(this);
        mListener = null;
    }

    /** Implements {@link IdentityManager.Observer}. */
    @Override
    public void onPrimaryAccountChanged(PrimaryAccountChangeEvent eventDetails) {
        if (eventDetails.getEventTypeFor() != PrimaryAccountChangeEvent.Type.CLEARED) {
            return;
        }
        // The user signed out while the snackbar was shown, so the "Undo" action is no longer
        // valid. This will call onDismissNoAction() if the snackbar is still shown or queued.
        mSnackbarManager.dismissSnackbars(this);
        // Cleanup the resources in case the snackbar was not found.
        mIdentityManager.removeObserver(this);
        mListener = null;
    }

    /** Shows a snackbar if the sign-in was successful, allowing the user to undo the action. */
    public static void showUndoSnackbarIfNeeded(
            Activity activity,
            Profile profile,
            @SigninAccessPoint int signinAccessPoint,
            @Nullable SnackbarManager snackbarManager,
            Listener listener,
            SigninAndHistorySyncCoordinator.Result result) {
        if (snackbarManager == null) {
            // TODO(crbug.com/437039311): Once SnackbarManager configuration is hardened and always
            // non-null, remove @Nullable and null check.
            Log.e(TAG, "SnackbarManager for displaying sign-in confirmation snackbar is null");
            return;
        }

        if (!result.hasSignedIn || !snackbarManager.canShowSnackbar()) {
            return;
        }

        // TODO(crbug.com/437039311): No snackbar shown if the sign-in is cancelled by the user
        // with the X button.
        IdentityManager identityManager =
                assumeNonNull(IdentityServicesProvider.get().getIdentityManager(profile));
        @Nullable AccountInfo accountInfo = identityManager.getPrimaryAccountInfo();
        if (accountInfo == null) {
            return;
        }
        String email = accountInfo.getEmail();
        SigninSnackbarController controller =
                new SigninSnackbarController(
                        activity,
                        profile,
                        getSignoutReason(signinAccessPoint),
                        HistorySyncHelper.getForProfile(profile),
                        snackbarManager,
                        identityManager,
                        assertNonNull(listener));
        Snackbar snackbar =
                Snackbar.make(
                        activity.getString(R.string.snackbar_signed_in_as, email),
                        controller,
                        Snackbar.TYPE_ACTION,
                        Snackbar.UMA_SIGN_IN);
        snackbar.setAction(activity.getString(R.string.snackbar_undo_signin), result);
        // Start observing before showing the snackbar, since showSnackbar() may synchronously
        // dismiss it, which would call onDismissNoAction().
        identityManager.addObserver(controller);
        snackbarManager.showSnackbar(snackbar);
    }

    static @SignoutReason int getSignoutReason(@SigninAccessPoint int signinAccessPoint) {
        return switch (signinAccessPoint) {
            case SigninAccessPoint.BOOKMARK_MANAGER ->
                    SignoutReason.USER_TAPPED_UNDO_RIGHT_AFTER_SIGN_IN_FROM_BOOKMARKS;
            case SigninAccessPoint.NTP_FEED_TOP_PROMO ->
                    SignoutReason.USER_TAPPED_UNDO_RIGHT_AFTER_SIGN_IN_FROM_NTP;
            case SigninAccessPoint.RECENT_TABS ->
                    SignoutReason.USER_TAPPED_UNDO_RIGHT_AFTER_SIGN_IN_FROM_RECENT_TABS;
            case SigninAccessPoint.SETTINGS_AUTOFILL_AND_PASSWORDS ->
                    SignoutReason.USER_TAPPED_UNDO_RIGHT_AFTER_SIGN_IN_FROM_AUTOFILL_AND_PASSWORDS;
            default ->
                    throw new IllegalStateException("Forbidden access point: " + signinAccessPoint);
        };
    }
}
