// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ProgressBar;

import androidx.annotation.MainThread;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modaldialog.ModalDialogProperties.Controller;
import org.chromium.ui.modelutil.PropertyModel;

/** Class to decouple ConfirmSyncDataStateMachine from UI code and dialog management. */
public class ConfirmSyncDataStateMachineDelegate {
    /**
     * Listener to receive events from progress dialog. If the dialog is not dismissed by showing
     * other dialog or calling {@link ConfirmSyncDataStateMachineDelegate#dismissAllDialogs},
     * then {@link #onCancel} will be called once.
     */
    interface ProgressDialogListener {
        /** This method is called when user cancels the dialog in any way. */
        void onCancel();
    }

    /**
     * Listener to receive events from timeout dialog. If the dialog is not dismissed by showing
     * other dialog or calling {@link ConfirmSyncDataStateMachineDelegate#dismissAllDialogs},
     * then either {@link #onCancel} or {@link #onRetry} will be called once.
     */
    interface TimeoutDialogListener {
        /** This method is called when user cancels the dialog in any way. */
        void onCancel();

        /** This method is called when user clicks retry button. */
        void onRetry();
    }

    /** A Progress Dialog that is shown while account management policy is being fetched. */
    private static final class ProgressDialogCoordinator {
        private final ProgressDialogListener mListener;
        private final PropertyModel mModel;
        private final ModalDialogManager mDialogManager;

        @MainThread
        private ProgressDialogCoordinator(
                Context context,
                ModalDialogManager dialogManager,
                ProgressDialogListener listener) {
            final View view =
                    LayoutInflater.from(context).inflate(R.layout.signin_progress_bar_dialog, null);

            mListener = listener;
            mModel =
                    new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                            .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                            .with(
                                    ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                    context.getString(R.string.cancel))
                            .with(ModalDialogProperties.CUSTOM_VIEW, view)
                            .with(ModalDialogProperties.CONTROLLER, createController())
                            .build();
            mDialogManager = dialogManager;
            mDialogManager.showDialog(mModel, ModalDialogType.APP);
        }

        @MainThread
        private void dismissDialog() {
            mDialogManager.dismissDialog(mModel, DialogDismissalCause.UNKNOWN);
        }

        private Controller createController() {
            return new Controller() {
                @Override
                public void onClick(PropertyModel model, int buttonType) {
                    if (buttonType == ButtonType.NEGATIVE) {
                        mDialogManager.dismissDialog(
                                mModel, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                    }
                }

                @Override
                public void onDismiss(PropertyModel model, int dismissalCause) {
                    if (dismissalCause == DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE
                            || dismissalCause == DialogDismissalCause.NEGATIVE_BUTTON_CLICKED) {
                        mListener.onCancel();
                    }
                }
            };
        }
    }

    /** A Timeout Dialog that is shown if account management policy fetch times out. */
    private static final class TimeoutDialogCoordinator {
        private final TimeoutDialogListener mListener;
        private final PropertyModel mModel;
        private final ModalDialogManager mDialogManager;

        @MainThread
        private TimeoutDialogCoordinator(
                Context context, ModalDialogManager dialogManager, TimeoutDialogListener listener) {
            mListener = listener;
            mModel =
                    new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                            .with(
                                    ModalDialogProperties.TITLE,
                                    context.getString(R.string.sign_in_timeout_title))
                            .with(
                                    ModalDialogProperties.MESSAGE_PARAGRAPH_1,
                                    context.getString(R.string.sign_in_timeout_message))
                            .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                            .with(
                                    ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                    context.getString(R.string.try_again))
                            .with(
                                    ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                    context.getString(R.string.cancel))
                            .with(ModalDialogProperties.CONTROLLER, createController())
                            .build();
            mDialogManager = dialogManager;
            mDialogManager.showDialog(mModel, ModalDialogType.APP);
        }

        @MainThread
        private void dismissDialog() {
            mDialogManager.dismissDialog(mModel, DialogDismissalCause.UNKNOWN);
        }

        private Controller createController() {
            return new Controller() {
                @Override
                public void onClick(PropertyModel model, int buttonType) {
                    if (buttonType == ButtonType.POSITIVE) {
                        mListener.onRetry();
                        mDialogManager.dismissDialog(
                                mModel, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                    } else if (buttonType == ButtonType.NEGATIVE) {
                        mDialogManager.dismissDialog(
                                mModel, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                    }
                }

                @Override
                public void onDismiss(PropertyModel model, int dismissalCause) {
                    if (dismissalCause == DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE
                            || dismissalCause == DialogDismissalCause.NEGATIVE_BUTTON_CLICKED) {
                        mListener.onCancel();
                    }
                }
            };
        }
    }

    private final ModalDialogManager mModalDialogManager;
    private final Context mContext;
    private final Profile mProfile;

    private @Nullable ProgressDialogCoordinator mProgressDialogCoordinator;
    private @Nullable TimeoutDialogCoordinator mTimeoutDialogCoordinator;
    private @Nullable ConfirmImportSyncDataDialogCoordinator
            mConfirmImportSyncDataDialogCoordinator;
    private @Nullable ConfirmManagedSyncDataDialogCoordinator
            mConfirmManagedSyncDataDialogCoordinator;

    public ConfirmSyncDataStateMachineDelegate(
            Context context, Profile profile, ModalDialogManager modalDialogManager) {
        mContext = context;
        mProfile = profile;
        mModalDialogManager = modalDialogManager;
    }

    /**
     * Shows progress dialog. Will dismiss other dialogs shown, if any.
     *
     * @param listener The {@link ProgressDialogListener} that will be notified about user actions.
     */
    void showFetchManagementPolicyProgressDialog(ProgressDialogListener listener) {
        dismissAllDialogs();
        mProgressDialogCoordinator =
                new ProgressDialogCoordinator(mContext, mModalDialogManager, listener);
    }

    /**
     * Shows timeout dialog. Will dismiss other dialogs shown, if any.
     *
     * @param listener The {@link TimeoutDialogListener} that will be notified about user actions.
     */
    void showFetchManagementPolicyTimeoutDialog(TimeoutDialogListener listener) {
        dismissAllDialogs();
        mTimeoutDialogCoordinator =
                new TimeoutDialogCoordinator(mContext, mModalDialogManager, listener);
    }

    /**
     * Shows ConfirmImportSyncDataDialog that gives the user the option to
     * merge data between the account they are attempting to sign in to and the
     * account they are currently signed into, or to keep the data separate.
     * This dialog is shown before signing out the current sync account.
     *
     * @param listener        Callback to be called if the user completes the dialog (as opposed to
     *                        hitting cancel).
     * @param oldAccountName  The previous sync account name.
     * @param newAccountName  The potential next sync account name.
     */
    void showConfirmImportSyncDataDialog(
            ConfirmImportSyncDataDialogCoordinator.Listener listener,
            String oldAccountName,
            String newAccountName) {
        dismissAllDialogs();
        boolean isCurrentAccountManaged =
                IdentityServicesProvider.get().getSigninManager(mProfile).getManagementDomain()
                        != null;
        boolean usesSplitStoresAndUPMForLocal =
                PasswordManagerUtilBridge.usesSplitStoresAndUPMForLocal(UserPrefs.get(mProfile));
        mConfirmImportSyncDataDialogCoordinator =
                new ConfirmImportSyncDataDialogCoordinator(
                        mContext,
                        mModalDialogManager,
                        listener,
                        oldAccountName,
                        newAccountName,
                        isCurrentAccountManaged,
                        usesSplitStoresAndUPMForLocal);
    }

    /**
     * Shows {@link ConfirmManagedSyncDataDialogCoordinator} when signing in to a managed account
     * (either through sign in or when switching accounts).
     * @param listener Callback for result.
     * @param domain The domain of the managed account.
     */
    void showSignInToManagedAccountDialog(
            ConfirmManagedSyncDataDialogCoordinator.Listener listener, String domain) {
        dismissAllDialogs();
        mConfirmManagedSyncDataDialogCoordinator =
                new ConfirmManagedSyncDataDialogCoordinator(
                        mContext, mModalDialogManager, listener, domain);
    }

    /** Dismisses all dialogs. */
    void dismissAllDialogs() {
        if (mProgressDialogCoordinator != null) {
            mProgressDialogCoordinator.dismissDialog();
            mProgressDialogCoordinator = null;
        }
        if (mTimeoutDialogCoordinator != null) {
            mTimeoutDialogCoordinator.dismissDialog();
            mTimeoutDialogCoordinator = null;
        }
        if (mConfirmImportSyncDataDialogCoordinator != null) {
            mConfirmImportSyncDataDialogCoordinator.dismissDialog();
            mConfirmImportSyncDataDialogCoordinator = null;
        }
        if (mConfirmManagedSyncDataDialogCoordinator != null) {
            mConfirmManagedSyncDataDialogCoordinator.dismissDialog();
            mConfirmManagedSyncDataDialogCoordinator = null;
        }
    }

    ProgressBar getProgressBarViewForTesting() {
        return mProgressDialogCoordinator
                .mModel
                .get(ModalDialogProperties.CUSTOM_VIEW)
                .findViewById(R.id.progress_bar);
    }
}
