// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import androidx.activity.ComponentActivity;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;

/**
 * Shows a snackbar after a successful sign-in, which allows the user to undo the sign-in. This
 * snackbar mentions the signed-in user's email and offers an "Undo" action button.
 */
@NullMarked
class SigninSnackbarController implements SnackbarManager.SnackbarController {

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

    private @Nullable Listener mListener;

    private SigninSnackbarController(Listener listener) {
        mListener = listener;
    }

    /** Implements {@link SnackbarController}. actionData will contain signinFlowResult */
    @Override
    public void onAction(@Nullable Object actionData) {
        // TODO(crbug.com/437039311):
        // - Disable history sync if it was enabled during the flow.
        // - Signout.
        // - Show signout snackbar, but not the confirmation dialog.
        assertNonNull(mListener).onUndoSignin();
        mListener = null;
    }

    @Override
    public void onDismissNoAction(@Nullable Object actionData) {
        mListener = null;
    }

    /** Shows a snackbar if the sign-in was successful, allowing the user to undo the action. */
    public static void showUndoSnackbarIfNeeded(
            ComponentActivity activity,
            Profile profile,
            SnackbarManager snackbarManager,
            Listener listener,
            @SigninAndHistorySyncCoordinator.Result int result) {
        if (result == SigninAndHistorySyncCoordinator.Result.COMPLETED) {
            // TODO(crbug.com/437039311): No snackbar shown if user doesn't actually sign-in in the
            // flow (e.g. if only history sync opt-in was achieved), and not shown if the sign-in is
            // cancelled by the user with the X button.
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
                            new SigninSnackbarController(assertNonNull(listener)),
                            Snackbar.TYPE_ACTION,
                            Snackbar.UMA_SIGN_IN);
            // TODO(crbug.com/437039311): pass history sync completion state
            snackbar.setAction(activity.getString(R.string.snackbar_undo_signin), result);
            snackbarManager.showSnackbar(snackbar);
        }
    }
}
