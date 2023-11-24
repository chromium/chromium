// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.util.Optional;

class OtpVerificationDialogProperties {
    /** Interface for the mediator to be notified of view actions. */
    interface ViewDelegate {
        /**
         * Notifies the mediator of a text changed event in the edit text field.
         *
         * @param s The current text in the edit text field.
         */
        void onTextChanged(CharSequence s);

        /** Notifies the mediator that the resend link was clicked. */
        void onResendLinkClicked();
    }

    static final int ANIMATION_DURATION_MS = 250;
    static final int DELAY_BETWEEN_CONFIRMATION_SHOWN_AND_DISMISSAL_MS = 250;

    static final ReadableIntPropertyKey OTP_LENGTH = new ReadableIntPropertyKey();

    static final ReadableObjectPropertyKey<String> EDIT_TEXT_HINT =
            new ReadableObjectPropertyKey<>();

    static final WritableObjectPropertyKey<ViewDelegate> VIEW_DELEGATE =
            new WritableObjectPropertyKey<>();

    // |EDIT_TEXT| is a one-to-one mapping of the edit text on the dialog. Empty |EDIT_TEXT|
    // indicates that there should be no edit text shown on the dialog, while
    // |OTP_ERROR_MESSAGE| with a value indicates that value should be displayed on the dialog.
    static final WritableObjectPropertyKey<Optional<CharSequence>> EDIT_TEXT =
            new WritableObjectPropertyKey<>();

    // |OTP_ERROR_MESSAGE| is a one-to-one mapping of the error message on the dialog. Empty
    // |OTP_ERROR_MESSAGE| indicates that there should be no error message shown on the dialog,
    // while |OTP_ERROR_MESSAGE| with a value indicates that value should be displayed on the
    // dialog.
    static final WritableObjectPropertyKey<Optional<String>> OTP_ERROR_MESSAGE =
            new WritableObjectPropertyKey<>();

    static final WritableObjectPropertyKey<String> SHOW_CONFIRMATION =
            new WritableObjectPropertyKey<>();

    static final WritableBooleanPropertyKey SHOW_PROGRESS_BAR_OVERLAY =
            new WritableBooleanPropertyKey();

    static final PropertyKey[] ALL_KEYS = {
        OTP_LENGTH,
        EDIT_TEXT,
        EDIT_TEXT_HINT,
        VIEW_DELEGATE,
        OTP_ERROR_MESSAGE,
        SHOW_CONFIRMATION,
        SHOW_PROGRESS_BAR_OVERLAY
    };
}
