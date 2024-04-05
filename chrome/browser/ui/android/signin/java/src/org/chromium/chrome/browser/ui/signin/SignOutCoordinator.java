// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.content.Context;

import androidx.annotation.IntDef;
import androidx.annotation.MainThread;
import androidx.annotation.Nullable;
import androidx.fragment.app.FragmentManager;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** A coordinator to handle sign-out. */
public class SignOutCoordinator {
    /**
     * Starts the sign-out flow. The caller must verify existence of a signed-in account and whether
     * sign-out is allowed before calling. Child users may only call this method if there is an
     * account with {@link ConsentLevel#SYNC}. It can show three different UIs depending on user
     * state:
     * <li>A snackbar indicating user has signed-out.
     * <li>A confirmation dialog indicating user has unsaved data.
     * <li>A confirmation dialog indicating that user may be signed-out as a side-effect of some
     *     action (e.g., toggling 'Allow Chrome sign-in').
     *
     * @param context Context to create the view.
     * @param profile The Profile to sign out of.
     * @param fragmentManager FragmentManager used by {@link SignOutDialogCoordinator}.
     * @param dialogManager A ModalDialogManager that manages the dialog.
     * @param signOutReason The access point to sign out from.
     * @param onSignOut A {@link Runnable} to run when the user presses the confirm button. Will be
     *     called on the UI thread when the sign-out flow finishes. If sign-out fails it will not be
     *     called.
     */
    @MainThread
    public static void startSignOutFlow(
            Context context,
            Profile profile,
            FragmentManager fragmentManager,
            ModalDialogManager dialogManager,
            SnackbarManager snackbarManager,
            @SignoutReason int signOutReason,
            @Nullable Runnable onSignOut) {
        ThreadUtils.assertOnUiThread();
        validateSignOutReason(profile, signOutReason);

        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(profile);
        if (!identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)) {
            throw new IllegalStateException("There is no signed-in account");
        }
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(profile);
        switch (getUiState(identityManager)) {
            case UiState.SNACK_BAR -> signOutAndShowSnackbar(
                    context, snackbarManager, signinManager, signOutReason, onSignOut);
            case UiState.UNSAVED_DATA -> showUnsavedDataDialog(
                    context, dialogManager, signinManager, signOutReason);
            case UiState.CLEAR_CHROME_DATA -> showSignOutDialog(context, dialogManager);
            case UiState.LEGACY_DIALOG -> SignOutDialogCoordinator.show(
                    context, profile, fragmentManager, dialogManager, signOutReason, onSignOut);
        }
    }

    @IntDef({
        UiState.SNACK_BAR,
        UiState.UNSAVED_DATA,
        UiState.CLEAR_CHROME_DATA,
        UiState.LEGACY_DIALOG
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface UiState {
        int SNACK_BAR = 0;
        int UNSAVED_DATA = 1;
        int CLEAR_CHROME_DATA = 2;
        int LEGACY_DIALOG = 3;
    }

    private static void validateSignOutReason(Profile profile, @SignoutReason int signOutReason) {
        switch (signOutReason) {
            case SignoutReason.USER_CLICKED_SIGNOUT_FROM_CLEAR_BROWSING_DATA_PAGE:
            case SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS:
                assert !profile.isChild() : "Child accounts can only revoke sync consent";
                return;
            case SignoutReason.USER_CLICKED_REVOKE_SYNC_CONSENT_SETTINGS:
                assert !ChromeFeatureList.isEnabled(
                        ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS);
                assert profile.isChild() : "Regular accounts can't just revoke sync consent";
                return;
            default:
                throw new IllegalArgumentException("Invalid signOutReason: " + signOutReason);
        }
    }

    private static @UiState int getUiState(IdentityManager identityManager) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
                || identityManager.hasPrimaryAccount(ConsentLevel.SYNC)) {
            return UiState.LEGACY_DIALOG;
        }
        if (hasUnsavedData()) {
            return UiState.UNSAVED_DATA;
        }
        if (shouldShowSignOutDialog()) {
            return UiState.CLEAR_CHROME_DATA;
        }
        return UiState.SNACK_BAR;
    }

    private static boolean hasUnsavedData() {
        // TODO(crbug.com/41493791): Implement logic for checking unsaved data.
        return false;
    }

    private static boolean shouldShowSignOutDialog() {
        // TODO(crbug.com/328395437): Implement clear data dialog.
        return false;
    }

    private static void showUnsavedDataDialog(
            Context context,
            ModalDialogManager dialogManager,
            SigninManager signinManager,
            @SignoutReason int signOutReason) {
        // TODO(crbug.com/41493791): Implement unsaved data dialog.
    }

    private static void showSignOutDialog(Context context, ModalDialogManager dialogManager) {
        // TODO(crbug.com/328395437): Implement clear data dialog.
    }

    private static void signOutAndShowSnackbar(
            Context context,
            SnackbarManager snackbarManager,
            SigninManager signinManager,
            @SignoutReason int signOutReason,
            @Nullable Runnable onSignOut) {
        signOut(
                signinManager,
                signOutReason,
                () -> {
                    snackbarManager.showSnackbar(
                            Snackbar.make(
                                    context.getString(R.string.sign_out_snackbar_message),
                                    /* controller= */ null,
                                    Snackbar.TYPE_ACTION,
                                    Snackbar.UMA_SIGN_OUT));
                    if (onSignOut != null) {
                        onSignOut.run();
                    }
                });
    }

    private static void signOut(
            SigninManager signinManager, @SignoutReason int signOutReason, Runnable onSignOut) {
        signinManager.runAfterOperationInProgress(
                () -> {
                    if (!signinManager.isSignOutAllowed()) {
                        // Sign out may not be allowed by the time we get here since it's
                        // asynchronous. In that case return early instead.
                        return;
                    }
                    SigninManager.SignOutCallback signOutCallback =
                            () -> PostTask.runOrPostTask(TaskTraits.UI_DEFAULT, onSignOut);
                    signinManager.signOut(
                            signOutReason, signOutCallback, /* forceWipeUserData= */ true);
                });
    }
}
