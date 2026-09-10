// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import androidx.activity.OnBackPressedCallback;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.enterprise_signals_disclaimer.EnterpriseSignalsDisclaimerHost.DismissalCause;
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
    private @Nullable Consumer<@DismissalCause Integer> mDialogDismissedCallback;
    private @Nullable @DismissalCause Integer mDismissalCause;

    public ModalDialogDisclaimerHost(
            ModalDialogManager modalDialogManager,
            EnterpriseSignalsDisclaimerView view,
            Consumer<@DismissalCause Integer> onDialogDismissedCallback) {
        mModalDialogManager = modalDialogManager;
        mDialogDismissedCallback = onDialogDismissedCallback;
        PropertyModel dialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .with(ModalDialogProperties.CUSTOM_VIEW, view)
                        .with(ModalDialogProperties.CONTROLLER, this)
                        .with(
                                ModalDialogProperties.APP_MODAL_DIALOG_BACK_PRESS_HANDLER,
                                createBackPressedCallback())
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
    public void dismiss(@DismissalCause int dismissalCause) {
        mIsActive = false;
        mDismissalCause = dismissalCause;
        mModalDialogManager.dismissDialog(
                mDialogModel, DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED);
    }

    @Override
    public void destroy() {
        mDialogDismissedCallback = null;
        if (mIsActive) {
            mModalDialogManager.dismissDialog(
                    mDialogModel, DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED);
        }
        mIsActive = false;
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
            @DismissalCause
            int cause =
                    (mDismissalCause != null)
                            ? mDismissalCause
                            : getDismissalCauseFromDialogDismissalCause(dismissalCause);
            var callback = mDialogDismissedCallback;
            mDialogDismissedCallback = null;
            callback.accept(cause);
        }
    }

    private static @DismissalCause int getDismissalCauseFromDialogDismissalCause(
            @DialogDismissalCause int dismissalCause) {
        assert dismissalCause != DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED;
        if (dismissalCause == DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE) {
            // Back press is handled by the OnBackPressedCallback.
            return DismissalCause.DISMISSED_BY_TAP_OUTSIDE;
        } else {
            return DismissalCause.DISMISSED_WITHOUT_EXPLICIT_USER_ACTION;
        }
    }

    // Because `onDialogDismiss` bundles touch outside and back press together, we need to handle
    // back press separately to get the correct dismissal cause.
    private OnBackPressedCallback createBackPressedCallback() {
        return new OnBackPressedCallback(/* enabled= */ true) {
            @Override
            public void handleOnBackPressed() {
                dismiss(EnterpriseSignalsDisclaimerHost.DismissalCause.DISMISSED_BY_BACK_PRESS);
            }
        };
    }
}
