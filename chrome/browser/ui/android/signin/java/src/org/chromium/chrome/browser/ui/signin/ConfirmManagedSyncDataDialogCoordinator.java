// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.content.Context;

import androidx.annotation.MainThread;

import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modaldialog.ModalDialogProperties.Controller;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A Coordinator to display the dialogs the user may encounter when switching to/from or signing
 * into/out of a managed account.
 */
public class ConfirmManagedSyncDataDialogCoordinator {
    /**
     * A listener to allow the Dialog to report on the action taken. Either {@link
     * Listener#onConfirm} or {@link Listener#onCancel} will be called once.
     */
    public interface Listener {
        /** The user has accepted the dialog. */
        void onConfirm();

        /**
         * The user has cancelled the dialog either through a negative response or by dismissing it.
         */
        void onCancel();
    }

    private final Listener mListener;
    private final PropertyModel mModel;
    private final ModalDialogManager mDialogManager;

    /**
     * Creates {@link ConfirmManagedSyncDataDialogCoordinator} when signing in to a managed account
     * (either through sign in or when switching accounts) and shows the dialog.
     *
     * @param context Context to create the view.
     * @param dialogManager ModalDialogManager to show the dialog.
     * @param listener Callback for result.
     * @param managedDomain The domain of the managed account.
     */
    @MainThread
    public ConfirmManagedSyncDataDialogCoordinator(
            Context context,
            ModalDialogManager dialogManager,
            Listener listener,
            String managedDomain) {
        mListener = listener;
        mModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(
                                ModalDialogProperties.TITLE,
                                context.getString(R.string.sign_in_managed_account))
                        .with(
                                ModalDialogProperties.MESSAGE_PARAGRAPH_1,
                                context.getString(
                                        R.string.managed_signin_with_user_policy_subtitle,
                                        managedDomain))
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                context.getString(R.string.continue_button))
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                context.getString(R.string.cancel))
                        .with(ModalDialogProperties.CONTROLLER, createController())
                        .build();
        mDialogManager = dialogManager;

        mDialogManager.showDialog(mModel, ModalDialogType.APP);
    }

    /** Dismisses confirm managed sync data dialog. */
    @MainThread
    public void dismissDialog() {
        mDialogManager.dismissDialog(mModel, DialogDismissalCause.UNKNOWN);
    }

    private Controller createController() {
        return new Controller() {
            @Override
            public void onClick(PropertyModel model, int buttonType) {
                if (buttonType == ButtonType.POSITIVE) {
                    mListener.onConfirm();
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
