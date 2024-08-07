// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.os.Handler;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * This class takes care of the various dialogs that must be shown when the user changes the
 * account they are syncing to (either directly, or by signing in to a new account). Most of the
 * complexity is due to many of the decisions getting answered through callbacks.
 *
 * This class progresses along the following state machine:
 *
 *       E-----\  G--\
 *       ^     |  ^  |
 *       |     v  |  v
 * A->B->C->D->+->F->H
 *    |        ^
 *    v        |
 *    \--------/
 *
 * Where:
 * A - Start
 * B - Decision: progress to C if the user signed in previously to a different account, F otherwise.
 * C - Decision: progress to E if we are switching from a managed account, D otherwise.
 * D - Action: show Import Data Dialog.
 * E - Action: show Switching from Managed Account Dialog.
 * F - Decision: progress to G if we are switching to a managed account, H otherwise.
 * G - Action: show Switching to Managed Account Dialog.
 * H - End: perform {@link ConfirmImportSyncDataDialogCoordinator.Listener#onConfirm} with the
 * result of the Import Data Dialog, if displayed or true if switching from a managed account.
 *
 * At any dialog, the user can cancel the dialog and end the whole process (resulting in
 * {@link ConfirmImportSyncDataDialogCoordinator.Listener#onCancel}).
 */
public class ConfirmSyncDataStateMachine
        implements ConfirmImportSyncDataDialogCoordinator.Listener,
                ConfirmManagedSyncDataDialogCoordinator.Listener {
    @IntDef({
        State.BEFORE_OLD_ACCOUNT_DIALOG,
        State.BEFORE_NEW_ACCOUNT_DIALOG,
        State.AFTER_NEW_ACCOUNT_DIALOG,
        State.DONE
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface State {
        int BEFORE_OLD_ACCOUNT_DIALOG = 0; // Start of state B.
        int BEFORE_NEW_ACCOUNT_DIALOG = 1; // Start of state F.
        int AFTER_NEW_ACCOUNT_DIALOG = 2; // Start of state H.
        int DONE = 4;
    }

    /**
     * Callback for completion of the {@link ConfirmSyncDataStateMachine}. TODO(crbug.com/40159777):
     * Change this method to package internal after modularization
     */
    public interface Listener {
        /**
         * The state machine has completed and the state is done, or all necessary confirmations
         * were given and the sign-in flow can proceed.
         *
         * @param wipeData Whether the user requested that existing data should be wiped.
         * @param acceptedAccountManagement Whether the user accepted account management.
         */
        void onConfirm(boolean wipeData, boolean acceptedAccountManagement);

        /**
         * The state machine is cancelled or the user cancels the sign-in process
         * of the dialogs.
         */
        void onCancel();
    }

    private @State int mState = State.BEFORE_OLD_ACCOUNT_DIALOG;

    private static final int ACCOUNT_CHECK_TIMEOUT_MS = 30000;

    private final Profile mProfile;
    private final Listener mListener;
    private final @Nullable String mOldAccountName;
    private final String mNewAccountName;
    private final ConfirmSyncDataStateMachineDelegate mDelegate;
    private final Handler mHandler = new Handler();

    private boolean mWipeData;
    private Boolean mNewAccountManaged;
    private Runnable mCheckTimeoutRunnable;

    /**
     * Create and run state machine, displaying the appropriate dialogs.
     *
     * @param profile The {@link Profile} associated with the sync data.
     * @param delegate the delegate responsible of showing dialogs
     * @param oldAccountName the name of the last signed in account or null
     * @param newAccountName the name of the account user is signing in with
     * @param listener the listener to receive the result of this state machine
     */
    public ConfirmSyncDataStateMachine(
            Profile profile,
            ConfirmSyncDataStateMachineDelegate delegate,
            @Nullable String oldAccountName,
            String newAccountName,
            Listener listener) {
        ThreadUtils.assertOnUiThread();
        assert !TextUtils.isEmpty(newAccountName) : "New account name must be provided.";

        mProfile = profile;
        mDelegate = delegate;
        mOldAccountName = oldAccountName;
        mNewAccountName = newAccountName;
        mListener = listener;

        // New account management status isn't needed right now, but fetching it
        // can take a few seconds, so we kick it off early.
        requestNewAccountManagementStatus();

        progress();
    }

    /**
     * Cancels this state machine, dismissing any dialogs being shown.
     * @param isBeingDestroyed whether state machine is being cancelled because enclosing UI object
     *         is being destroyed. This state machine will not invoke callbacks or dismiss dialogs
     *         if isBeingDestroyed is true.
     */
    public void cancel(boolean isBeingDestroyed) {
        ThreadUtils.assertOnUiThread();

        cancelTimeout();
        mState = State.DONE;

        if (isBeingDestroyed) return;
        mListener.onCancel();
        mDelegate.dismissAllDialogs();
    }

    /**
     * This will progress the state machine, by moving the state along and then by either calling
     * itself directly or creating a dialog. If the dialog is dismissed or answered negatively the
     * entire flow is over, if it is answered positively one of the onConfirm functions is called
     * and this function is called again.
     */
    private void progress() {
        switch (mState) {
            case State.BEFORE_OLD_ACCOUNT_DIALOG:
                mState = State.BEFORE_NEW_ACCOUNT_DIALOG;

                if (TextUtils.isEmpty(mOldAccountName) || mNewAccountName.equals(mOldAccountName)) {
                    // If there is no old account or the user is just logging back into whatever
                    // they were previously logged in as, progress past the old account checks.
                    progress();
                } else {
                    // This will call back into onConfirm(boolean wipeData) on success.
                    mDelegate.showConfirmImportSyncDataDialog(
                            this, mOldAccountName, mNewAccountName);
                }

                break;
            case State.BEFORE_NEW_ACCOUNT_DIALOG:
                mState = State.AFTER_NEW_ACCOUNT_DIALOG;
                if (mNewAccountManaged != null) {
                    // No need to show dialog if account management status is already known
                    handleNewAccountManagementStatus();
                } else {
                    showProgressDialog();
                    scheduleTimeout();
                }
                break;
            case State.AFTER_NEW_ACCOUNT_DIALOG:
                mState = State.DONE;
                mListener.onConfirm(mWipeData, mNewAccountManaged);
                break;
            case State.DONE:
                throw new IllegalStateException("Can't progress from DONE state!");
        }
    }

    private void requestNewAccountManagementStatus() {
        IdentityServicesProvider.get()
                .getSigninManager(mProfile)
                .isAccountManaged(mNewAccountName, this::setIsNewAccountManaged);
    }

    private void setIsNewAccountManaged(Boolean isManaged) {
        assert isManaged != null;
        mNewAccountManaged = isManaged;
        if (mState == State.AFTER_NEW_ACCOUNT_DIALOG) {
            cancelTimeout();
            handleNewAccountManagementStatus();
        }
    }

    private void handleNewAccountManagementStatus() {
        assert mNewAccountManaged != null;
        assert mState == State.AFTER_NEW_ACCOUNT_DIALOG;

        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(mProfile);
        // Signin will already have show the account management dialog before the confirm sync
        // screen is shown, so we shouldn't show it again.
        if (mNewAccountManaged && !signinManager.getUserAcceptedAccountManagement()) {
            // Show 'logging into managed account' dialog
            // This will call back into onConfirm on success.
            mDelegate.showSignInToManagedAccountDialog(
                    this,
                    IdentityServicesProvider.get()
                            .getSigninManager(mProfile)
                            .extractDomainName(mNewAccountName));
        } else {
            mDelegate.dismissAllDialogs();
            progress();
        }
    }

    private void showProgressDialog() {
        mDelegate.showFetchManagementPolicyProgressDialog(this::onCancel);
    }

    private void scheduleTimeout() {
        if (mCheckTimeoutRunnable == null) {
            mCheckTimeoutRunnable = this::checkTimeout;
        }
        mHandler.postDelayed(mCheckTimeoutRunnable, ACCOUNT_CHECK_TIMEOUT_MS);
    }

    private void cancelTimeout() {
        if (mCheckTimeoutRunnable == null) {
            return;
        }
        mHandler.removeCallbacks(mCheckTimeoutRunnable);
        mCheckTimeoutRunnable = null;
    }

    private void checkTimeout() {
        assert mState == State.AFTER_NEW_ACCOUNT_DIALOG;
        assert mNewAccountManaged == null;

        mDelegate.showFetchManagementPolicyTimeoutDialog(
                new ConfirmSyncDataStateMachineDelegate.TimeoutDialogListener() {
                    @Override
                    public void onCancel() {
                        ConfirmSyncDataStateMachine.this.onCancel();
                    }

                    @Override
                    public void onRetry() {
                        requestNewAccountManagementStatus();
                        scheduleTimeout();
                        showProgressDialog();
                    }
                });
    }

    /** {@link ConfirmImportSyncDataDialogCoordinator.Listener} implementation. */
    @Override
    public void onConfirm(boolean wipeData) {
        mWipeData = wipeData;
        progress();
    }

    /** {@link ConfirmManagedSyncDataDialogCoordinator.Listener} implementation. */
    @Override
    public void onConfirm() {
        progress();
    }

    /**
     * {@link ConfirmImportSyncDataDialogCoordinator.Listener} & {@link
     * ConfirmManagedSyncDataDialogCoordinator.Listener} implementation.
     */
    @Override
    public void onCancel() {
        cancel(false);
    }
}
