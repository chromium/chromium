// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.device_lock;

import android.accounts.Account;
import android.app.Activity;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

public class DeviceLockDialogController implements DeviceLockCoordinator.Delegate {
    /** The {@link ModalDialogManager} which launches the Missing Device Lock dialog. */
    private final @NonNull ModalDialogManager mModalDialogManager;

    /**The {@link PropertyModel} of the underlying dialog where the view would be shown.*/
    private PropertyModel mModalDialogPropertyModel;

    private final Runnable mOnDeviceLockReady;
    private final Runnable mOnDeviceLockRefused;
    private final DeviceLockCoordinator mDeviceLockCoordinator;
    private final @DeviceLockActivityLauncher.Source String mSource;

    /**
     * The modal dialog controller to detect events on the dialog but it's not needed in this
     * case.
     */
    private final ModalDialogProperties.Controller mModalDialogController =
            new ModalDialogProperties.Controller() {
                @Override
                public void onClick(PropertyModel model, int buttonType) {}

                @Override
                public void onDismiss(PropertyModel model, int dismissalCause) {}
            };

    /**
     * Launcher to manage the Device Lock dialog to inform the user about data privacy and prompting
     * the user to create a device lock.
     *
     * @param onDeviceLockReady Callback to run if the device has a device lock set and the user has
     *     chosen to continue.
     * @param onDeviceLockRefused Callback to run if user has decided to dismiss the dialog without
     *     setting a device lock.
     * @param windowAndroid Used to launch Intents with callbacks.
     * @param activity The activity hosting the dialog.
     * @param modalDialogManager The {@link ModalDialogManager} to host the Missing Device Lock
     *     dialog.
     * @param account The account that will be used for the reauthentication challenge, or null if
     *     reauthentication is not needed.
     * @param requireDeviceLockReauthentication Whether or not the reauthentication of the device
     *     lock credentials should be required (if a device lock is already present).
     * @param source Which source flow the user took to arrive at the device lock UI.
     */
    public DeviceLockDialogController(
            Runnable onDeviceLockReady,
            Runnable onDeviceLockRefused,
            WindowAndroid windowAndroid,
            Activity activity,
            @NonNull ModalDialogManager modalDialogManager,
            @Nullable Account account,
            boolean requireDeviceLockReauthentication,
            @DeviceLockActivityLauncher.Source String source) {
        mSource = source;
        mOnDeviceLockReady = onDeviceLockReady;
        mOnDeviceLockRefused = onDeviceLockRefused;
        mModalDialogManager = modalDialogManager;
        if (requireDeviceLockReauthentication) {
            mDeviceLockCoordinator =
                    new DeviceLockCoordinator(this, windowAndroid, activity, account);
        } else {
            mDeviceLockCoordinator =
                    new DeviceLockCoordinator(
                            this,
                            windowAndroid,
                            /* deviceLockAuthenticatorBridge= */ null,
                            activity,
                            account);
        }
    }

    /** Show the Device Lock UI in a modal dialog. */
    public void showDialog() {
        mModalDialogManager.showDialog(
                mModalDialogPropertyModel,
                ModalDialogManager.ModalDialogType.APP,
                ModalDialogManager.ModalDialogPriority.HIGH);
    }

    /**
     * Dismiss the dialog showing the Missing Device Lock UI.
     *
     * @param dismissalCause The {@link DialogDismissalCause} stating the reason why the Device
     *                       Lock dialog is being dismissed.
     */
    public void hideDialog(@DialogDismissalCause int dismissalCause) {
        mModalDialogManager.dismissDialog(mModalDialogPropertyModel, dismissalCause);
        mDeviceLockCoordinator.destroy();
    }

    @Override
    public void setView(View view) {
        mModalDialogPropertyModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, mModalDialogController)
                        .with(ModalDialogProperties.CUSTOM_VIEW, view)
                        .with(
                                ModalDialogProperties.DIALOG_STYLES,
                                ModalDialogProperties.DialogStyles.NORMAL)
                        .build();
    }

    @Override
    public void onDeviceLockReady() {
        mOnDeviceLockReady.run();
    }

    @Override
    public void onDeviceLockRefused() {
        mOnDeviceLockRefused.run();
    }

    @Override
    public @DeviceLockActivityLauncher.Source String getSource() {
        return mSource;
    }
}
