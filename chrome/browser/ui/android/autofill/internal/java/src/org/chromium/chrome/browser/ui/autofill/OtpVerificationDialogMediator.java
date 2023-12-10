// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static org.chromium.chrome.browser.ui.autofill.OtpVerificationDialogProperties.DELAY_BETWEEN_CONFIRMATION_SHOWN_AND_DISMISSAL_MS;
import static org.chromium.chrome.browser.ui.autofill.OtpVerificationDialogProperties.EDIT_TEXT;
import static org.chromium.chrome.browser.ui.autofill.OtpVerificationDialogProperties.OTP_ERROR_MESSAGE;
import static org.chromium.chrome.browser.ui.autofill.OtpVerificationDialogProperties.OTP_LENGTH;
import static org.chromium.chrome.browser.ui.autofill.OtpVerificationDialogProperties.SHOW_CONFIRMATION;
import static org.chromium.chrome.browser.ui.autofill.OtpVerificationDialogProperties.SHOW_PROGRESS_BAR_OVERLAY;
import static org.chromium.chrome.browser.ui.autofill.OtpVerificationDialogProperties.VIEW_DELEGATE;

import android.os.Handler;

import org.chromium.chrome.browser.ui.autofill.OtpVerificationDialogCoordinator.Delegate;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Optional;

class OtpVerificationDialogMediator
        implements ModalDialogProperties.Controller, OtpVerificationDialogProperties.ViewDelegate {
    private final ModalDialogManager mModalDialogManager;
    private PropertyModel mModalDialogModel;
    private Delegate mDelegate;
    private PropertyModel mOtpVerificationDialogModel;

    OtpVerificationDialogMediator(
            ModalDialogManager modalDialogManager,
            PropertyModel.Builder dialogModelBuilder,
            Delegate delegate) {
        mModalDialogManager = modalDialogManager;
        mModalDialogModel = dialogModelBuilder.with(ModalDialogProperties.CONTROLLER, this).build();
        mDelegate = delegate;
    }

    @Override
    public void onDismiss(PropertyModel model, int dismissalCause) {
        mDelegate.onDialogDismissed();
    }

    @Override
    public void onClick(PropertyModel model, int buttonType) {
        switch (buttonType) {
            case ModalDialogProperties.ButtonType.POSITIVE:
                Optional<CharSequence> editTextOptional =
                        mOtpVerificationDialogModel.get(EDIT_TEXT);
                // Safety check, this should always be true.
                if (editTextOptional.isPresent()) {
                    showProgressBarOverlay();
                    mDelegate.onConfirm(editTextOptional.get().toString());
                }
                break;
            case ModalDialogProperties.ButtonType.NEGATIVE:
                mModalDialogManager.dismissDialog(
                        model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                break;
        }
    }

    @Override
    public void onTextChanged(CharSequence s) {
        mModalDialogModel.set(
                ModalDialogProperties.POSITIVE_BUTTON_DISABLED,
                s.length() != mOtpVerificationDialogModel.get(OTP_LENGTH));
        mOtpVerificationDialogModel.set(OTP_ERROR_MESSAGE, Optional.empty());
        mOtpVerificationDialogModel.set(EDIT_TEXT, Optional.of(s));
    }

    @Override
    public void onResendLinkClicked() {
        clearEditText();
        onNewOtpRequested();
    }

    /**
     * Show the OtpVerification dialog.
     *
     * @param dialogViewModel The model for the dialog view.
     */
    void show(PropertyModel dialogViewModel) {
        setDialogViewModel(dialogViewModel);
        mModalDialogManager.showDialog(mModalDialogModel, ModalDialogManager.ModalDialogType.TAB);
    }

    /** Dismiss the dialog if is already showing. */
    void dismissDialog() {
        mModalDialogManager.dismissDialog(
                mModalDialogModel, DialogDismissalCause.DISMISSED_BY_NATIVE);
    }

    /** Clear the text in the Edit Text field. */
    void clearEditText() {
        mOtpVerificationDialogModel.set(EDIT_TEXT, Optional.empty());
    }

    /**
     * Shows the progress bar overlay for the dialog. This method is called once the users clicks
     * the accept button.
     */
    void showProgressBarOverlay() {
        mOtpVerificationDialogModel.set(SHOW_PROGRESS_BAR_OVERLAY, true);
        mModalDialogModel.set(ModalDialogProperties.POSITIVE_BUTTON_DISABLED, true);
    }

    /** Show an error message for the submitted otp. */
    void showOtpErrorMessage(Optional<String> errorMessage) {
        mOtpVerificationDialogModel.set(SHOW_PROGRESS_BAR_OVERLAY, false);
        mOtpVerificationDialogModel.set(OTP_ERROR_MESSAGE, errorMessage);
        mModalDialogModel.set(ModalDialogProperties.POSITIVE_BUTTON_DISABLED, true);
    }

    /**
     * Shows the confirmation message and dismisses the dialog. This method is called when the
     * server returns a success response after the user clicked the accept button.
     *
     * @param confirmationMessage The confirmation message that gets shown on successful server
     * response.
     */
    void showConfirmationAndDismissDialog(String confirmationMessage) {
        mOtpVerificationDialogModel.set(SHOW_CONFIRMATION, confirmationMessage);
        new Handler()
                .postDelayed(
                        this::dismissDialog, DELAY_BETWEEN_CONFIRMATION_SHOWN_AND_DISMISSAL_MS);
    }

    void onNewOtpRequested() {
        mDelegate.onNewOtpRequested();
    }

    private void setDialogViewModel(PropertyModel dialogViewModel) {
        mOtpVerificationDialogModel = dialogViewModel;
        mOtpVerificationDialogModel.set(VIEW_DELEGATE, this);
    }
}
