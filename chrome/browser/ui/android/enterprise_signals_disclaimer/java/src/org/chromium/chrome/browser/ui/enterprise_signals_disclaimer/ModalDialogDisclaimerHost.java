// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.function.Consumer;

/**
 * Implementation of {@link EnterpriseSignalsDisclaimerHost} using {@link ModalDialogManager} to
 * display the disclaimer in a modal dialog.
 */
@NullMarked
class ModalDialogDisclaimerHost
        implements EnterpriseSignalsDisclaimerHost, ModalDialogProperties.Controller {
    private final ModalDialogManager mModalDialogManager;
    private final PropertyModel mDialogModel;
    private boolean mIsActive;
    private @Nullable Consumer<Boolean> mDialogDismissedCallback;

    public ModalDialogDisclaimerHost(
            ModalDialogManager modalDialogManager,
            EnterpriseSignalsDisclaimerView view,
            Consumer<Boolean> dialogDismissedCallback) {
        mModalDialogManager = modalDialogManager;
        mDialogDismissedCallback = dialogDismissedCallback;
        PropertyModel dialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .with(ModalDialogProperties.CUSTOM_VIEW, view)
                        .with(ModalDialogProperties.CONTROLLER, this)
                        .build();
        mDialogModel = dialogModel;
        mIsActive = false;
    }

    // EnterpriseSignalsDisclaimerHost implementation.
    @Override
    public void show() {
        mModalDialogManager.showDialog(
                mDialogModel,
                ModalDialogManager.ModalDialogType.APP,
                ModalDialogManager.ModalDialogPriority.HIGH);
        mIsActive = true;
    }

    @Override
    public boolean isActive() {
        return mIsActive;
    }

    @Override
    public void hide() {
        mModalDialogManager.dismissDialog(
                mDialogModel, DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED);
        mIsActive = false;
    }

    @Override
    public void destroy() {
        mDialogDismissedCallback = null;
        hide();
    }

    // ModalDialogProperties.Controller implementation.
    @Override
    public void onClick(PropertyModel model, @ButtonType int buttonType) {
        assert false
                : "This dialog uses a custom view with its own buttons, this should never be"
                        + " called";
    }

    @Override
    public void onDismiss(PropertyModel model, @DialogDismissalCause int dismissalCause) {
        mIsActive = false;
        if (mDialogDismissedCallback != null) {
            mDialogDismissedCallback.accept(
                    dismissalCause == DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);
            mDialogDismissedCallback = null;
        }
    }
}
