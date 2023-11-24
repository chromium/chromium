// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.device_lock;

import static org.chromium.components.browser_ui.device_lock.DeviceLockBridge.DEVICE_LOCK_PAGE_HAS_BEEN_PASSED;

import android.content.Context;
import android.content.SharedPreferences;
import android.view.LayoutInflater;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** The coordinator handles the creation, update, and interaction of the missing device lock UI. */
public class MissingDeviceLockCoordinator {
    /** The {@link ModalDialogManager} which launches the Missing Device Lock dialog. */
    private final @NonNull ModalDialogManager mModalDialogManager;

    /**The {@link PropertyModel} of the underlying dialog where the view would be shown.*/
    private final PropertyModel mModalDialogPropertyModel;

    private final MissingDeviceLockMediator mMediator;
    private final MissingDeviceLockView mView;
    private final PropertyModelChangeProcessor mPropertyModelChangeProcessor;

    // Values of the histogram recording events related to the removal of the device lock.
    @IntDef({
        MissingDeviceLockDialogEvent.DIALOG_SHOWN,
        MissingDeviceLockDialogEvent.CONTINUE_WITHOUT_DEVICE_LOCK,
        MissingDeviceLockDialogEvent.DEVICE_LOCK_RESTORED,
        MissingDeviceLockDialogEvent.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface MissingDeviceLockDialogEvent {
        int DIALOG_SHOWN = 0;
        int CONTINUE_WITHOUT_DEVICE_LOCK = 1;
        int DEVICE_LOCK_RESTORED = 2;
        int COUNT = 3;
    }

    /**
     * The modal dialog controller to detect events on the dialog but it's not needed in our
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
     * Constructs a coordinator for the Missing Device Lock page.
     *
     * @param onContinueWithoutDeviceLock Called when the user has decided to continue without
     *                                    recreating a device lock.
     * @param context The context hosting this page.
     */
    public MissingDeviceLockCoordinator(
            Callback<Boolean> onContinueWithoutDeviceLock,
            Context context,
            ModalDialogManager modalDialogManager) {
        mView = MissingDeviceLockView.create(LayoutInflater.from(context));

        mMediator =
                new MissingDeviceLockMediator(
                        (wipeAllData) ->
                                continueWithoutDeviceLock(wipeAllData, onContinueWithoutDeviceLock),
                        context);

        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mMediator.getModel(), mView, MissingDeviceLockViewBinder::bind);
        mModalDialogManager = modalDialogManager;

        mModalDialogPropertyModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, mModalDialogController)
                        .with(ModalDialogProperties.CUSTOM_VIEW, mView)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, false)
                        .with(
                                ModalDialogProperties.DIALOG_STYLES,
                                ModalDialogProperties.DialogStyles.NORMAL)
                        .build();
    }

    @VisibleForTesting
    void continueWithoutDeviceLock(
            Boolean wipeAllData, Callback<Boolean> onContinueWithoutDeviceLock) {
        onContinueWithoutDeviceLock.onResult(wipeAllData);
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        prefs.edit().remove(DEVICE_LOCK_PAGE_HAS_BEEN_PASSED).apply();
        RecordHistogram.recordEnumeratedHistogram(
                "Android.Automotive.DeviceLockRemovalDialogEvent",
                MissingDeviceLockDialogEvent.CONTINUE_WITHOUT_DEVICE_LOCK,
                MissingDeviceLockDialogEvent.COUNT);
    }

    /** Releases the resources used by the coordinator. */
    public void destroy() {
        mPropertyModelChangeProcessor.destroy();
    }

    /** Show the Missing Device Lock UI in a modal dialog. */
    public void showDialog() {
        showMissingDeviceLockDialog();
    }

    /** Dismiss the dialog showing the Missing Device Lock UI. */
    public void hideDialog(@DialogDismissalCause int dismissalCause) {
        dismissMissingDeviceLockDialog(dismissalCause);
    }

    /** Method to show the Missing Device Lock dialog. */
    private void showMissingDeviceLockDialog() {
        mModalDialogManager.showDialog(
                mModalDialogPropertyModel,
                ModalDialogManager.ModalDialogType.APP,
                ModalDialogManager.ModalDialogPriority.VERY_HIGH);
        RecordHistogram.recordEnumeratedHistogram(
                "Android.Automotive.DeviceLockRemovalDialogEvent",
                MissingDeviceLockDialogEvent.DIALOG_SHOWN,
                MissingDeviceLockDialogEvent.COUNT);
    }

    /**
     * Method to hide the Missing Device Lock dialog.
     *
     * @param dismissalCause The {@link DialogDismissalCause} stating the reason why the Missing
     *                       Device Lockdialog is being dismissed.
     */
    private void dismissMissingDeviceLockDialog(@DialogDismissalCause int dismissalCause) {
        mModalDialogManager.dismissDialog(mModalDialogPropertyModel, dismissalCause);
        destroy();
    }
}
