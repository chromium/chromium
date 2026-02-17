// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;

import androidx.annotation.IntDef;
import androidx.annotation.MainThread;
import androidx.fragment.app.FragmentManager;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserActionableError;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.text.NumberFormat;
import java.util.stream.IntStream;

/** A coordinator to handle sign-out. */
@NullMarked
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
     * @param snackbarManager The manager for displaying snackbars at the bottom of the activity.
     * @param signinAndHistorySyncActivityLauncher launcher used to launch the {@link
     *     SigninAndHistorySyncActivity}.
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
            SigninAndHistorySyncActivityLauncher signinAndHistorySyncActivityLauncher,
            @SignoutReason int signOutReason,
            boolean showConfirmDialog,
            Runnable onSignOut) {
        startSignOutFlow(
                context,
                profile,
                fragmentManager,
                dialogManager,
                snackbarManager,
                signinAndHistorySyncActivityLauncher,
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
     * @param snackbarManager SnackbarManager for displaying snackbars at the bottom of the
     *     activity.
     * @param signinAndHistorySyncActivityLauncher launcher used to launch the {@link
     *     SigninAndHistorySyncActivity}.
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
            SigninAndHistorySyncActivityLauncher signinAndHistorySyncActivityLauncher,
            @SignoutReason int signOutReason,
            boolean showConfirmDialog,
            Runnable onSignOut,
            boolean suppressSnackbar) {
        ThreadUtils.assertOnUiThread();
        assert snackbarManager != null;
        assert onSignOut != null;
        validateSignOutReason(profile, signOutReason);

        IdentityManager identityManager = getSignedInIdentityManager(profile);
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(profile);
        assumeNonNull(signinManager);
        SyncService syncService = SyncServiceFactory.getForProfile(profile);
        assumeNonNull(syncService);
        @UserActionableError int userActionableError = syncService.getUserActionableError();
        syncService.getTypesWithUnsyncedData(
                unsyncedTypes -> {
                    @UiState
                    int uiState =
                            getUiState(
                                    identityManager,
                                    signinManager,
                                    !unsyncedTypes.isEmpty(),
                                    showConfirmDialog,
                                    userActionableError);
                    switch (uiState) {
                        case UiState.SNACK_BAR ->
                                signOutAndShowSnackbar(
                                        context,
                                        snackbarManager,
                                        signinManager,
                                        syncService,
                                        signOutReason,
                                        onSignOut,
                                        suppressSnackbar);
                        case UiState.UNSAVED_DATA ->
                                showUnsavedDataDialog(
                                        context,
                                        dialogManager,
                                        signinManager,
                                        userActionableError,
                                        signOutReason,
                                        onSignOut);
                        case UiState.SHOW_CONFIRM_DIALOG ->
                                showConfirmDialog(
                                        context,
                                        dialogManager,
                                        snackbarManager,
                                        signinManager,
                                        syncService,
                                        signOutReason,
                                        onSignOut);
                        case UiState.LEGACY_DIALOG ->
                                SignOutDialogCoordinator.show(
                                        context,
                                        profile,
                                        fragmentManager,
                                        dialogManager,
                                        signOutReason,
                                        onSignOut);
                        case UiState.FULLSCREEN_DIALOG ->
                                signOutAndShowFullscreenDialog(
                                        context,
                                        profile,
                                        signinAndHistorySyncActivityLauncher,
                                        signinManager,
                                        signOutReason,
                                        onSignOut);
                    }
                    if (uiState != UiState.SNACK_BAR) {
                        RecordHistogram.recordBooleanHistogram(
                                "Sync.BookmarksLimitExceededOnSignoutPrompt",
                                userActionableError
                                        == UserActionableError.BOOKMARKS_LIMIT_EXCEEDED);
                    }
                });
    }

    /**
     * Starts a silent sign-out flow that only shows a snackbar upon completion. This bypasses the
     * standard signout confirmation dialog.
     *
     * <p>This should ONLY be used when caller is sure there's no unsynced data, such as reversing a
     * sign-in action immediately after it was completed (e.g., via an "Undo" button on a snackbar).
     * For all other sign-out scenarios, use {@link #startSignOutFlow()} to ensure the user can save
     * their work.
     *
     * @param context Context to create the view.
     * @param profile The Profile to sign out of.
     * @param snackbarManager The manager for displaying snackbars at the bottom of the activity.
     * @param signOutReason The access point to sign out from.
     * @param onSignOut A {@link Runnable} is called on the UI thread when the sign-out flow
     *     finishes. If sign-out fails it will not be called.
     */
    @MainThread
    public static void undoSignInWithSnackbar(
            Context context,
            Profile profile,
            SnackbarManager snackbarManager,
            @SignoutReason int signOutReason,
            Runnable onSignOut) {
        ThreadUtils.assertOnUiThread();
        switch (signOutReason) {
            case SignoutReason.USER_TAPPED_UNDO_RIGHT_AFTER_SIGN_IN_FROM_BOOKMARKS:
            case SignoutReason.USER_TAPPED_UNDO_RIGHT_AFTER_SIGN_IN_FROM_NTP:
            case SignoutReason.USER_TAPPED_UNDO_RIGHT_AFTER_SIGN_IN_FROM_RECENT_TABS:
                break;
            default:
                throw new IllegalArgumentException("Invalid signOutReason: " + signOutReason);
        }
        getSignedInIdentityManager(profile);
        assert snackbarManager != null;
        assert onSignOut != null;

        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(profile);
        assumeNonNull(signinManager);
        SyncService syncService = SyncServiceFactory.getForProfile(profile);
        assumeNonNull(syncService);

        syncService.getTypesWithUnsyncedData(
                unsyncedTypes -> {
                    if (!unsyncedTypes.isEmpty()) {
                        throw new IllegalStateException(
                                "This sign-out flow should not be used if there is unsaved data.");
                    }
                });
        signOutAndShowSnackbar(
                context,
                snackbarManager,
                signinManager,
                syncService,
                signOutReason,
                onSignOut,
                false);
    }

    // TODO: b/325654229 - This method should be private. It's temporarily made public as a work
    // around for b/343933167.
    /** Shows the snackbar which is shown upon signing out. */
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
                        .setDefaultLines(false));
    }

    @IntDef({
        UiState.SNACK_BAR,
        UiState.UNSAVED_DATA,
        UiState.SHOW_CONFIRM_DIALOG,
        UiState.LEGACY_DIALOG,
        UiState.FULLSCREEN_DIALOG,
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface UiState {
        int SNACK_BAR = 0;
        int UNSAVED_DATA = 1;
        int SHOW_CONFIRM_DIALOG = 2;
        int LEGACY_DIALOG = 3;
        int FULLSCREEN_DIALOG = 4;
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

    private static IdentityManager getSignedInIdentityManager(Profile profile) {
        IdentityManager identityManager =
                assumeNonNull(IdentityServicesProvider.get().getIdentityManager(profile));
        if (!identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)) {
            throw new IllegalStateException("There is no signed-in account");
        }
        return identityManager;
    }

    private static @UiState int getUiState(
            IdentityManager identityManager,
            SigninManager signinManager,
            boolean hasUnsavedData,
            boolean showConfirmDialog,
            @UserActionableError int userActionableError) {
        if (SigninFeatureMap.isEnabled(SigninFeatures.SUPPORT_FORCED_SIGNIN_POLICY)
                && signinManager.isForceSigninEnabled()) {
            return UiState.FULLSCREEN_DIALOG;
        }
        if (userActionableError == UserActionableError.BOOKMARKS_LIMIT_EXCEEDED) {
            return UiState.UNSAVED_DATA;
        }
        if (identityManager.hasPrimaryAccount(ConsentLevel.SYNC)) {
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
            @UserActionableError int userActionableError,
            @SignoutReason int signOutReason,
            Runnable onSignOut) {
        String message = context.getString(R.string.sign_out_unsaved_data_message);
        if (userActionableError == UserActionableError.BOOKMARKS_LIMIT_EXCEEDED) {
            message =
                    context.getString(
                            R.string.chrome_signout_confirmation_prompt_too_many_bookmarks_body,
                            NumberFormat.getIntegerInstance()
                                    .format(SyncService.SYNC_BOOKMARKS_LIMIT));
        }
        final PropertyModel model =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(
                                ModalDialogProperties.TITLE,
                                context.getString(R.string.sign_out_unsaved_data_title))
                        .with(ModalDialogProperties.MESSAGE_PARAGRAPH_1, message)
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

    private static void signOutAndShowFullscreenDialog(
            Context context,
            Profile profile,
            SigninAndHistorySyncActivityLauncher signinAndHistorySyncActivityLauncher,
            SigninManager signinManager,
            @SignoutReason int signOutReason,
            Runnable onSignOut) {
        signOut(
                signinManager,
                signOutReason,
                () -> {
                    PostTask.runOrPostTask(
                            TaskTraits.UI_DEFAULT,
                            () -> {
                                onSignOut.run();
                                FullscreenSigninPromoLauncher.launchPromoIfForced(
                                        context, profile, signinAndHistorySyncActivityLauncher);
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
