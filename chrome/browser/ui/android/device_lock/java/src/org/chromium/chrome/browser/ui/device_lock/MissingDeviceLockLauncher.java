// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.device_lock;

import android.app.KeyguardManager;
import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.password_manager.PasswordStoreBridge;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninManager.DataWipeOption;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;

public class MissingDeviceLockLauncher {
    private Context mContext;
    private Profile mProfile;
    private ModalDialogManager mModalDialogManager;
    private MissingDeviceLockCoordinator mMissingDeviceLockCoordinator;
    private PasswordStoreBridge mPasswordStoreBridge;

    /**
     * Launcher to show and handle the Missing Device Lock dialog to the user to prompt them to
     * recreate a device lock.
     *
     * @param context The context hosting the current activity.
     * @param profile The profile to clear data from.
     * @param modalDialogManager The {@link ModalDialogManager} to host the
     *        Missing Device Lock dialog.
     */
    public MissingDeviceLockLauncher(
            Context context, Profile profile, ModalDialogManager modalDialogManager) {
        mContext = context;
        mProfile = profile;
        mModalDialogManager = modalDialogManager;
        mMissingDeviceLockCoordinator = null;
    }

    /**
     * Prompts the user to recreate a device lock if one is expected but has been removed. If the
     * user chooses to proceed without a device lock, highly sensitive data will be deleted, with
     * the option to delete all personal data.
     *
     * @return The {@link MissingDeviceLockCoordinator} for the Missing Device Lock UI being shown.
     */
    public MissingDeviceLockCoordinator checkPrivateDataIsProtectedByDeviceLock() {
        KeyguardManager keyguardManager =
                (KeyguardManager) mContext.getSystemService(Context.KEYGUARD_SERVICE);

        // If device lock is present, record that to show an alert if it is later removed.
        if (keyguardManager.isDeviceSecure()) {
            // Hide the Missing Device Lock dialog if a device lock has been set (as prompted).
            if (mMissingDeviceLockCoordinator != null) {
                mMissingDeviceLockCoordinator.hideDialog(
                        DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED);
                RecordHistogram.recordEnumeratedHistogram(
                        "Android.Automotive.DeviceLockRemovalDialogEvent",
                        MissingDeviceLockCoordinator.MissingDeviceLockDialogEvent
                                .DEVICE_LOCK_RESTORED,
                        MissingDeviceLockCoordinator.MissingDeviceLockDialogEvent.COUNT);
                mMissingDeviceLockCoordinator = null;
            }
            ChromeSharedPreferences.getInstance()
                    .writeBoolean(ChromePreferenceKeys.DEVICE_LOCK_SHOW_ALERT_IF_REMOVED, true);
            return null;
        }

        // If the device lock has been removed, prompt the user with the missing device lock UI.
        if (mMissingDeviceLockCoordinator == null
                && ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.DEVICE_LOCK_SHOW_ALERT_IF_REMOVED, false)) {
            Callback<Boolean> onContinueWithoutDeviceLock =
                    (wipeAllData) ->
                            ensureSignOutAndDeleteSensitiveData(
                                    () ->
                                            mMissingDeviceLockCoordinator.hideDialog(
                                                    DialogDismissalCause.POSITIVE_BUTTON_CLICKED),
                                    wipeAllData);
            mMissingDeviceLockCoordinator =
                    new MissingDeviceLockCoordinator(
                            onContinueWithoutDeviceLock, mContext, mModalDialogManager);
            mMissingDeviceLockCoordinator.showDialog();
            return mMissingDeviceLockCoordinator;
        }

        return null;
    }

    /**
     * Signs out the user if applicable and deletes sensitive personal data.
     *
     * @param wipeDataCallback Callback to run after the data has been wiped.
     * @param wipeAllData Whether or not to wipe all personal data. If false, only passwords and
     *         credit cards will be deleted.
     */
    void ensureSignOutAndDeleteSensitiveData(Runnable wipeDataCallback, boolean wipeAllData) {
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(mProfile);
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(mProfile);

        signinManager.runAfterOperationInProgress(
                () -> {
                    if (identityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN) != null) {
                        signinManager.signOut(
                                SignoutReason.DEVICE_LOCK_REMOVED_ON_AUTOMOTIVE,
                                () -> {
                                    if (!wipeAllData) {
                                        deletePasswordsAndCreditCards();
                                    }
                                    wipeDataCallback.run();
                                },
                                wipeAllData);
                    } else {
                        if (wipeAllData) {
                            signinManager.wipeSyncUserData(
                                    wipeDataCallback, DataWipeOption.WIPE_ALL_PROFILE_DATA);
                        } else {
                            deletePasswordsAndCreditCards();
                            wipeDataCallback.run();
                        }
                    }
                    ChromeSharedPreferences.getInstance()
                            .writeBoolean(
                                    ChromePreferenceKeys.DEVICE_LOCK_SHOW_ALERT_IF_REMOVED, false);
                });
    }

    void setPasswordStoreBridgeForTesting(PasswordStoreBridge passwordStoreBridge) {
        mPasswordStoreBridge = passwordStoreBridge;
    }

    @VisibleForTesting
    PasswordStoreBridge getPasswordStoreBridge() {
        if (mPasswordStoreBridge == null) {
            mPasswordStoreBridge = new PasswordStoreBridge(mProfile);
        }
        return mPasswordStoreBridge;
    }

    private void deletePasswordsAndCreditCards() {
        getPasswordStoreBridge().clearAllPasswords();
        PersonalDataManagerFactory.getForProfile(mProfile).deleteAllLocalCreditCards();
    }

    /**
     * Set the Missing Device Lock coordinator for testing.
     *
     * @param missingDeviceLockCoordinator The coordinator to use for testing.
     */
    void setMissingDeviceLockCoordinatorForTesting(
            MissingDeviceLockCoordinator missingDeviceLockCoordinator) {
        mMissingDeviceLockCoordinator = missingDeviceLockCoordinator;
    }
}
