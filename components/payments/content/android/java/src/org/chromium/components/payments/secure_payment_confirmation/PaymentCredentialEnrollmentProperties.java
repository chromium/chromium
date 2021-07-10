// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.secure_payment_confirmation;

import android.graphics.drawable.Drawable;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;

/**
 * The properties of the SecurePaymentConfirmation Enrollment UI, which fully describe the state of
 * the UI.
 */
/* package */ class PaymentCredentialEnrollmentProperties {
    /** The payment icon for the UI. */
    /* package */ static final ReadableObjectPropertyKey<Drawable> PAYMENT_ICON =
            new ReadableObjectPropertyKey<>();

    /** The payment value of the UI. */
    /* package */ static final ReadableObjectPropertyKey<String> PAYMENT_INSTRUMENT_LABEL =
            new ReadableObjectPropertyKey<>();

    /** The callback when the cancel button is pressed. */
    /* package */ static final ReadableObjectPropertyKey<Runnable> CANCEL_BUTTON_CALLBACK =
            new ReadableObjectPropertyKey<>();

    /** The callback when the continue button is pressed. */
    /* package */ static final ReadableObjectPropertyKey<Runnable> CONTINUE_BUTTON_CALLBACK =
            new ReadableObjectPropertyKey<>();

    /** Whether the extra incognito text is visible. */
    /* package */ static final ReadableObjectPropertyKey<Boolean> INCOGNITO_TEXT_VISIBLE =
            new ReadableObjectPropertyKey<>();

    /* package */ static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {PAYMENT_ICON, PAYMENT_INSTRUMENT_LABEL, CONTINUE_BUTTON_CALLBACK,
                    CANCEL_BUTTON_CALLBACK, INCOGNITO_TEXT_VISIBLE};
}
