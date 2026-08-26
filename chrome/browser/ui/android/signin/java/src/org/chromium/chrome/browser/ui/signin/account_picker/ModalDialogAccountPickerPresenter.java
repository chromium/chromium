// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Presenter that displays the account picker inside a modal dialog. */
@NullMarked
class ModalDialogAccountPickerPresenter implements AccountPickerPresenter {
    private final ModalDialogManager mModalDialogManager;
    private final AccountPickerDismissalLogger mDismissalLogger;
    private final AccountPickerDelegate mAccountPickerDelegate;
    private final Runnable mOnDestroyCallback;
    private @Nullable PropertyModel mDialogModel;

    ModalDialogAccountPickerPresenter(
            ModalDialogManager modalDialogManager,
            AccountPickerDismissalLogger dismissalLogger,
            AccountPickerDelegate accountPickerDelegate,
            Runnable onDestroyCallback) {
        mModalDialogManager = modalDialogManager;
        mDismissalLogger = dismissalLogger;
        mAccountPickerDelegate = accountPickerDelegate;
        mOnDestroyCallback = onDestroyCallback;
    }

    @Override
    public void show(AccountPickerBottomSheetView view) {
        ModalDialogProperties.Controller dialogController =
                new ModalDialogProperties.Controller() {
                    @Override
                    public void onClick(PropertyModel model, int buttonType) {}

                    @Override
                    public void onDismiss(PropertyModel model, int dismissalCause) {
                        mDialogModel = null;
                        mDismissalLogger.logModalDialogDismissal(dismissalCause);
                        if (dismissalCause != DialogDismissalCause.ACTION_ON_CONTENT
                                && dismissalCause
                                        != DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED) {
                            mAccountPickerDelegate.onSignInCancel();
                        }
                        mOnDestroyCallback.run();
                    }
                };

        mDialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, dialogController)
                        .with(ModalDialogProperties.CUSTOM_VIEW, view.getContentView())
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .with(
                                ModalDialogProperties.DIALOG_STYLES,
                                ModalDialogProperties.DialogStyles.DIALOG_WHEN_LARGE)
                        .build();

        mModalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.APP);
    }

    @Override
    public void dismiss() {
        if (mDialogModel != null) {
            PropertyModel dialogModel = mDialogModel;
            mDialogModel = null;
            mModalDialogManager.dismissDialog(dialogModel, DialogDismissalCause.ACTION_ON_CONTENT);
        }
    }

    @Override
    public void destroy() {
        // Dialog destruction is handled via dismissal.
    }
}
