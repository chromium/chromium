// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.content.Context;

import androidx.annotation.IntDef;
import androidx.annotation.MainThread;
import androidx.fragment.app.FragmentManager;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.stream.IntStream;

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
     * @param showConfirmDialog Whether a confirm dialog should be shown before sign-out.
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
            boolean showConfirmDialog,
            Runnable onSignOut) {
        startSignOutFlow(
                context,
                profile,
                fragmentManager,
                dialogManager,
                snackbarManager,
                signOutReason,
                showConfirmDialog,
                onSignOut,
                false);
    }

    // TODO: crbug.com/343933167 - The @param suppressSnackbar removed upon being able to show the
    // settings
    // signout snackbar from here, which means after fixing b/343933167.
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
     * @param showConfirmDialog Whether a confirm dialog should be shown before sign-out.
     * @param onSignOut A {@link Runnable} to run when the user presses the confirm button. Will be
     *     called on the UI thread when the sign-out flow finishes. If sign-out fails it will not be
     *     called.
     * @param suppressSnackbar Indicates whether the snackbar should be suppressed.
     */
    @MainThread
    public static void startSignOutFlow(
            Context context,
            Profile profile,
            FragmentManager fragmentManager,
            ModalDialogManager dialogManager,
            SnackbarManager snackbarManager,
            @SignoutReason int signOutReason,
            boolean showConfirmDialog,
            Runnable onSignOut,
            boolean suppressSnackbar) {
        ThreadUtils.assertOnUiThread();
        assert snackbarManager != null;
        assert onSignOut != null;
        validateSignOutReason(profile, signOutReason);

        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(profile);
        if (!identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)) {
            throw new IllegalStateException("There is no signed-in account");
        }
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(profile);
        SyncService syncService = SyncServiceFactory.getForProfile(profile);
        syncService.getTypesWithUnsyncedData(
                unsyncedTypes -> {
                    switch (getUiState(
                            identityManager, !unsyncedTypes.isEmpty(), showConfirmDialog)) {
                        case UiState.SNACK_BAR -> signOutAndShowSnackbar(
                                context,
                                snackbarManager,
                                signinManager,
                                syncService,
                                signOutReason,
                                onSignOut,
                                suppressSnackbar);
                        case UiState.UNSAVED_DATA -> showUnsavedDataDialog(
                                context, dialogManager, signinManager, signOutReason, onSignOut);
                        case UiState.SHOW_CONFIRM_DIALOG -> showConfirmDialog(
                                context,
                                dialogManager,
                                snackbarManager,
                                signinManager,
                                syncService,
                                signOutReason,
                                onSignOut);
                        case UiState.LEGACY_DIALOG -> SignOutDialogCoordinator.show(
                                context,
                                profile,
                                fragmentManager,
                                dialogManager,
                                signOutReason,
                                onSignOut);
                    }
                });
    }

    // TODO: b/325654229 - This method should be private. It's temporarily made public as a work
    // around for b/343933167.
    /** Shows the sanckbar which is shown upon signing out. */
    public static void showSnackbar(
            Context context, SnackbarManager snackbarManager, SyncService syncService) {
        boolean anyTypeIsManagedByPolicy =
                IntStream.of(
                                UserSelectableType.AUTOFILL,
                                UserSelectableType.BOOKMARKS,
                                UserSelectableType.PASSWORDS,
                                UserSelectableType.PAYMENTS,
                                UserSelectableType.PREFERENCES,
                                UserSelectableType.READING_LIST,
                                UserSelectableType.HISTORY,
                                UserSelectableType.TABS)
                        .anyMatch(syncService::isTypeManagedByPolicy);
        boolean shouldShowEnterprisePolicyMessage =
                anyTypeIsManagedByPolicy || syncService.isSyncDisabledByEnterprisePolicy();
        snackbarManager.showSnackbar(
                Snackbar.make(
                                context.getString(
                                        shouldShowEnterprisePolicyMessage
                                                ? R.string
                                                        .account_settings_sign_out_snackbar_message_sync_disabled
                                                : R.string.sign_out_snackbar_message),
                                /* controller= */ null,
                                Snackbar.TYPE_ACTION,
                                Snackbar.UMA_SIGN_OUT)
                        .setSingleLine(false));
    }

    @IntDef({
        UiState.SNACK_BAR,
        UiState.UNSAVED_DATA,
        UiState.SHOW_CONFIRM_DIALOG,
        UiState.LEGACY_DIALOG
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface UiState {
        int SNACK_BAR = 0;
        int UNSAVED_DATA = 1;
        int SHOW_CONFIRM_DIALOG = 2;
        int LEGACY_DIALOG = 3;
    }

    private static void validateSignOutReason(Profile profile, @SignoutReason int signOutReason) {
        switch (signOutReason) {
            case SignoutReason.USER_CLICKED_SIGNOUT_FROM_CLEAR_BROWSING_DATA_PAGE:
            case SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS:
            case SignoutReason.USER_DISABLED_ALLOW_CHROME_SIGN_IN:
                assert !profile.isChild() : "Child accounts can only revoke sync consent";
                return;
            case SignoutReason.USER_CLICKED_REVOKE_SYNC_CONSENT_SETTINGS:
                assert profile.isChild() : "Regular accounts can't just revoke sync consent";
                return;
            default:
                throw new IllegalArgumentException("Invalid signOutReason: " + signOutReason);
        }
    }

    private static @UiState int getUiState(
            IdentityManager identityManager, boolean hasUnsavedData, boolean showConfirmDialog) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
                || identityManager.hasPrimaryAccount(ConsentLevel.SYNC)) {
            return UiState.LEGACY_DIALOG;
        }
        if (hasUnsavedData) {
            return UiState.UNSAVED_DATA;
        }
        if (showConfirmDialog) {
            return UiState.SHOW_CONFIRM_DIALOG;
        }
        return UiState.SNACK_BAR;
    }

    private static void showUnsavedDataDialog(
            Context context,
            ModalDialogManager dialogManager,
            SigninManager signinManager,
            @SignoutReason int signOutReason,
            Runnable onSignOut) {
        final PropertyModel model =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(
                                ModalDialogProperties.TITLE,
                                context.getString(R.string.sign_out_unsaved_data_title))
                        .with(
                                ModalDialogProperties.MESSAGE_PARAGRAPH_1,
                                context.getString(R.string.sign_out_unsaved_data_message))
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                context.getString(R.string.sign_out_unsaved_data_primary_button))
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                context.getString(R.string.cancel))
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .with(
                                ModalDialogProperties.CONTROLLER,
                                createController(
                                        dialogManager, signinManager, signOutReason, onSignOut))
                        .build();
        dialogManager.showDialog(model, ModalDialogManager.ModalDialogType.APP);
    }

    private static void showConfirmDialog(
            Context context,
            ModalDialogManager dialogManager,
            SnackbarManager snackbarManager,
            SigninManager signinManager,
            SyncService syncService,
            @SignoutReason int signOutReason,
            Runnable onSignOut) {
        ModalDialogProperties.Controller controller =
                createController(
                        dialogManager,
                        signinManager,
                        signOutReason,
                        () -> {
                            onSignOut.run();
                            showSnackbar(context, snackbarManager, syncService);
                        });
        final PropertyModel model =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(
                                ModalDialogProperties.TITLE,
                                context.getString(R.string.sign_out_title))
                        .with(
                                ModalDialogProperties.MESSAGE_PARAGRAPH_1,
                                context.getString(R.string.sign_out_message))
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                context.getString(R.string.sign_out))
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                context.getString(R.string.cancel))
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .with(ModalDialogProperties.CONTROLLER, controller)
                        .build();
        dialogManager.showDialog(model, ModalDialogManager.ModalDialogType.APP);
    }

    private static ModalDialogProperties.Controller createController(
            ModalDialogManager dialogManager,
            SigninManager signinManager,
            @SignoutReason int signOutReason,
            Runnable onSignOut) {
        return new ModalDialogProperties.Controller() {
            @Override
            public void onClick(PropertyModel model, int buttonType) {
                if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
                    signOut(
                            signinManager,
                            signOutReason,
                            () -> PostTask.runOrPostTask(TaskTraits.UI_DEFAULT, onSignOut));
                    dialogManager.dismissDialog(
                            model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                } else if (buttonType == ModalDialogProperties.ButtonType.NEGATIVE) {
                    dialogManager.dismissDialog(
                            model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                }
            }

            @Override
            public void onDismiss(PropertyModel model, int dismissalCause) {}
        };
    }

    private static void signOutAndShowSnackbar(
            Context context,
            SnackbarManager snackbarManager,
            SigninManager signinManager,
            SyncService syncService,
            @SignoutReason int signOutReason,
            Runnable onSignOut,
            boolean supressSnackbar) {
        signOut(
                signinManager,
                signOutReason,
                () -> {
                    PostTask.runOrPostTask(
                            TaskTraits.UI_DEFAULT,
                            () -> {
                                if (!supressSnackbar) {
                                    showSnackbar(context, snackbarManager, syncService);
                                }
                                onSignOut.run();
                            });
                });
    }

    private static void signOut(
            SigninManager signinManager,
            @SignoutReason int signOutReason,
            SigninManager.SignOutCallback signOutCallback) {
        signinManager.runAfterOperationInProgress(
                () -> {
                    if (!signinManager.isSignOutAllowed()) {
                        // Sign out may not be allowed by the time we get here since it's
                        // asynchronous. In that case return early instead.
                        return;
                    }
                    signinManager.signOut(
                            signOutReason, signOutCallback, /* forceWipeUserData= */ false);
                });
    }
}
