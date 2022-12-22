// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static org.chromium.chrome.browser.ui.autofill.OtpVerificationDialogProperties.EDIT_TEXT;
import static org.chromium.chrome.browser.ui.autofill.OtpVerificationDialogProperties.EDIT_TEXT_HINT;
import static org.chromium.chrome.browser.ui.autofill.OtpVerificationDialogProperties.OTP_ERROR_MESSAGE;
import static org.chromium.chrome.browser.ui.autofill.OtpVerificationDialogProperties.SHOW_CONFIRMATION;
import static org.chromium.chrome.browser.ui.autofill.OtpVerificationDialogProperties.SHOW_PROGRESS_BAR_OVERLAY;
import static org.chromium.chrome.browser.ui.autofill.OtpVerificationDialogProperties.VIEW_DELEGATE;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Optional;

/** Class responsible for binding the model and the view in OTP Verification Dialog. **/
class OtpVerificationDialogViewBinder {
    private OtpVerificationDialogViewBinder() {}

    static void bind(
            PropertyModel model, OtpVerificationDialogView dialogView, PropertyKey propertyKey) {
        if (propertyKey.equals(EDIT_TEXT)) {
            if (!model.get(EDIT_TEXT).isPresent()) {
                dialogView.clearEditText();
            }
        } else if (propertyKey.equals(EDIT_TEXT_HINT)) {
            dialogView.setEditTextHint(model.get(EDIT_TEXT_HINT));
        } else if (propertyKey.equals(VIEW_DELEGATE)) {
            dialogView.setViewDelegate(model.get(VIEW_DELEGATE));
        } else if (propertyKey.equals(OTP_ERROR_MESSAGE)) {
            Optional<String> errorMessage = model.get(OTP_ERROR_MESSAGE);
            if (errorMessage.isPresent()) {
                dialogView.showOtpErrorMessage(errorMessage);
            } else {
                dialogView.hideOtpErrorMessage();
            }
        } else if (propertyKey.equals(SHOW_CONFIRMATION)) {
            dialogView.showConfirmation(model.get(SHOW_CONFIRMATION));
        } else if (propertyKey.equals(SHOW_PROGRESS_BAR_OVERLAY)) {
            if (model.get(SHOW_PROGRESS_BAR_OVERLAY)) {
                dialogView.fadeInProgressBarOverlay();
            } else {
                dialogView.fadeOutProgressBarOverlay();
            }
        }
    }
}
