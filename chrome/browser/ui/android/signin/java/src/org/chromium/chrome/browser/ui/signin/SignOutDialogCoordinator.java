// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.app.Dialog;
import android.app.ProgressDialog;
import android.content.Context;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.CheckBox;
import android.widget.TextView;

import androidx.annotation.MainThread;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.fragment.app.DialogFragment;
import androidx.fragment.app.FragmentManager;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modaldialog.ModalDialogProperties.Controller;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A confirmation dialog for signing out and/or wiping device data. The checkbox to wipe data is not
 * shown for managed accounts. This dialog can be used to only turn off sync without signing out
 * child accounts that are syncing.
 */
final class SignOutDialogCoordinator {
    private static final String CLEAR_DATA_PROGRESS_DIALOG_TAG = "clear_data_progress";

    /**
     * A dialog with a spinner shown when the user decides to clear data on sign-out. The dialog
     * closes by itself once the data has been cleared.
     *
     * <p>TODO(crbug.com/41493791): Use ModalDialog instead.
     */
    public static class ClearDataProgressDialog extends DialogFragment {
        @Override
        public void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            // Don't allow the dialog to be recreated by Android, since it wouldn't ever be
            // dismissed after recreation.
            if (savedInstanceState != null) dismiss();
        }

        @Override
        public Dialog onCreateDialog(Bundle savedInstanceState) {
            setCancelable(false);
            ProgressDialog dialog = new ProgressDialog(getActivity());
            dialog.setTitle(getString(R.string.wiping_profile_data_title));
            dialog.setMessage(getString(R.string.wiping_profile_data_message));
            dialog.setIndeterminate(true);
            return dialog;
        }
    }

    private final Profile mProfile;
    private final FragmentManager mFragmentManager;
    private final ModalDialogManager mDialogManager;
    @SignoutReason int mSignOutReason;
    @Nullable private final Runnable mOnSignOut;
    private final CheckBox mCheckBox;
    private final PropertyModel mModel;

    /**
     * Shows a dialog asking users to confirm whether they want to sign out. Optionally in case of
     * non-managed users and users who are able to delete browser history, offer an option to clear
     * all local data from the device as part of sign out. Child accounts may not be able to sign
     * out (as Child accounts are signed in by force) and may only turn off sync with the option to
     * clear all local data provided they are syncing.
     *
     * @param context Context to create the view.
     * @param profile The Profile to sign out of.
     * @param fragmentManager FragmentManager to show ClearDataProgressDialog.
     * @param dialogManager A ModalDialogManager that manages the dialog.
     * @param signOutReason The access point to sign out from. Child accounts must use {@link
     *     SignoutReason#USER_CLICKED_REVOKE_SYNC_CONSENT_SETTINGS}. Other accounts must not use
     *     {@link SignoutReason#USER_CLICKED_REVOKE_SYNC_CONSENT_SETTINGS}.
     * @param onSignOut A {@link Runnable} to run when the user presses the confirm button. Will be
     *     called when the sign-out flow finishes. If sign-out fails it will not be called. It may
     *     be null.
     */
    @MainThread
    static void show(
            Context context,
            Profile profile,
            FragmentManager fragmentManager,
            ModalDialogManager dialogManager,
            @SignoutReason int signOutReason,
            @Nullable Runnable onSignOut) {
        ThreadUtils.assertOnUiThread();
        new SignOutDialogCoordinator(
                context, profile, fragmentManager, dialogManager, signOutReason, onSignOut);
    }

    private static void validateSignOutReason(Profile profile, @SignoutReason int signOutReason) {
        if (profile.isChild()
                && signOutReason != SignoutReason.USER_CLICKED_REVOKE_SYNC_CONSENT_SETTINGS) {
            throw new IllegalArgumentException("Child accounts can only revoke sync consent");
        }
        if (!profile.isChild()
                && signOutReason == SignoutReason.USER_CLICKED_REVOKE_SYNC_CONSENT_SETTINGS) {
            throw new IllegalArgumentException("Regular accounts can't just revoke sync consent");
        }
    }

    private static View inflateView(Context context, Profile profile) {
        final View view =
                LayoutInflater.from(context).inflate(R.layout.signout_wipe_storage_dialog, null);
        ((TextView) view.findViewById(android.R.id.message)).setText(getMessage(context, profile));

        return view;
    }

    private static @StringRes int getTitleRes(Profile profile, @SignoutReason int signOutReason) {
        if (!IdentityServicesProvider.get()
                .getIdentityManager(profile)
                .hasPrimaryAccount(ConsentLevel.SYNC)) {
            return R.string.signout_title;
        }
        final String managedDomain =
                IdentityServicesProvider.get().getSigninManager(profile).getManagementDomain();
        if (managedDomain != null) {
            return R.string.signout_managed_account_title;
        }
        switch (signOutReason) {
            case SignoutReason.USER_CLICKED_REVOKE_SYNC_CONSENT_SETTINGS:
                return R.string.turn_off_sync_title;
            default:
                return R.string.turn_off_sync_and_signout_title;
        }
    }

    private static String getMessage(Context context, Profile profile) {
        if (!IdentityServicesProvider.get()
                .getIdentityManager(profile)
                .hasPrimaryAccount(ConsentLevel.SYNC)) {
            return context.getString(R.string.signout_message);
        }
        final String managedDomain =
                IdentityServicesProvider.get().getSigninManager(profile).getManagementDomain();
        if (managedDomain != null) {
            return context.getString(R.string.signout_managed_account_message, managedDomain);
        }
        return context.getString(
                PasswordManagerUtilBridge.usesSplitStoresAndUPMForLocal(UserPrefs.get(profile))
                        ? R.string.turn_off_sync_and_signout_message_without_passwords
                        : R.string.turn_off_sync_and_signout_message);
    }

    private static int getCheckBoxVisibility(Profile profile) {
        // TODO(crbug.com/40820738): extract logic for whether data wiping is allowed into
        // SigninManager.
        final boolean allowDeletingData =
                UserPrefs.get(profile).getBoolean(Pref.ALLOW_DELETING_BROWSER_HISTORY);
        final boolean hasSyncConsent =
                IdentityServicesProvider.get()
                        .getIdentityManager(profile)
                        .hasPrimaryAccount(ConsentLevel.SYNC);
        final String managedDomain =
                IdentityServicesProvider.get().getSigninManager(profile).getManagementDomain();
        final boolean showCheckBox = (managedDomain == null) && allowDeletingData && hasSyncConsent;
        return showCheckBox ? View.VISIBLE : View.GONE;
    }

    @VisibleForTesting
    @MainThread
    SignOutDialogCoordinator(
            Context context,
            Profile profile,
            FragmentManager fragmentManager,
            ModalDialogManager dialogManager,
            @SignoutReason int signOutReason,
            @Nullable Runnable onSignOut) {
        validateSignOutReason(profile, signOutReason);
        mProfile = profile;
        mFragmentManager = fragmentManager;
        mDialogManager = dialogManager;
        mSignOutReason = signOutReason;
        mOnSignOut = onSignOut;

        final View view = inflateView(context, mProfile);
        mCheckBox = view.findViewById(R.id.remove_local_data);
        mCheckBox.setVisibility(getCheckBoxVisibility(mProfile));

        mModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(
                                ModalDialogProperties.TITLE,
                                context.getString(getTitleRes(profile, mSignOutReason)))
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                context.getString(R.string.continue_button))
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                context.getString(R.string.cancel))
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .with(ModalDialogProperties.CUSTOM_VIEW, view)
                        .with(ModalDialogProperties.CONTROLLER, createController())
                        .build();
        mDialogManager.showDialog(mModel, ModalDialogType.APP);
    }

    private Controller createController() {
        return new Controller() {
            @Override
            public void onClick(PropertyModel model, int buttonType) {
                if (buttonType == ButtonType.POSITIVE) {
                    if (mCheckBox.getVisibility() == View.VISIBLE) {
                        RecordHistogram.recordBooleanHistogram(
                                "Signin.UserRequestedWipeDataOnSignout", mCheckBox.isChecked());
                    }
                    boolean forceWipeData =
                            mCheckBox.getVisibility() == View.VISIBLE && mCheckBox.isChecked();
                    signOut(forceWipeData);
                    mDialogManager.dismissDialog(
                            mModel, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                } else if (buttonType == ButtonType.NEGATIVE) {
                    mDialogManager.dismissDialog(
                            mModel, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                }
            }

            @Override
            public void onDismiss(PropertyModel model, int dismissalCause) {}

            private void signOut(boolean forceWipeUserData) {
                final DialogFragment clearDataProgressDialog = new ClearDataProgressDialog();
                SigninManager.SignOutCallback dataWipeCallback =
                        new SigninManager.SignOutCallback() {
                            @Override
                            public void preWipeData() {
                                clearDataProgressDialog.show(
                                        mFragmentManager, CLEAR_DATA_PROGRESS_DIALOG_TAG);
                            }

                            @Override
                            public void signOutComplete() {
                                // TODO(crbug.com/40220998): deal with both the following edge cases
                                // (currently this code only deals with 1):
                                //
                                // 1) The parent activity showing the dialog is dismissed before
                                // signout completes.
                                // 2) The signout completes before the dialog is added.
                                if (clearDataProgressDialog.isAdded()) {
                                    clearDataProgressDialog.dismissAllowingStateLoss();
                                }
                                if (mOnSignOut != null) {
                                    mOnSignOut.run();
                                }
                            }
                        };
                SigninManager signinManager =
                        IdentityServicesProvider.get().getSigninManager(mProfile);
                signinManager.runAfterOperationInProgress(
                        () -> {
                            if (mSignOutReason
                                    == SignoutReason.USER_CLICKED_REVOKE_SYNC_CONSENT_SETTINGS) {
                                signinManager.revokeSyncConsent(
                                        mSignOutReason, dataWipeCallback, forceWipeUserData);
                            } else if (signinManager.isSignOutAllowed()) {
                                signinManager.signOut(
                                        mSignOutReason, dataWipeCallback, forceWipeUserData);
                            }
                        });
            }
        };
    }

    @VisibleForTesting
    @MainThread
    View getDialogViewForTesting() {
        return mModel.get(ModalDialogProperties.CUSTOM_VIEW);
    }

    @VisibleForTesting
    @MainThread
    void dismissDialogForTesting() {
        mDialogManager.dismissDialog(mModel, DialogDismissalCause.ACTIVITY_DESTROYED);
    }
}
