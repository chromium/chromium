// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.secure_payment_confirmation;

import android.graphics.drawable.Drawable;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.url.Origin;

/**
 * The properties of the SecurePaymentConfirmation Authn UI, which fully describe the state of the
 * UI.
 */
/* package */ class SecurePaymentConfirmationAuthnProperties {
    /** The store value of the UI. */
    /* package */ static final ReadableObjectPropertyKey<Origin> STORE_ORIGIN =
            new ReadableObjectPropertyKey<>();

    /** The payment icon for the UI. */
    /* package */ static final ReadableObjectPropertyKey<Drawable> PAYMENT_ICON =
            new ReadableObjectPropertyKey<>();

    /** The payment value of the UI. */
    /* package */ static final ReadableObjectPropertyKey<CharSequence> PAYMENT_INSTRUMENT_LABEL =
            new ReadableObjectPropertyKey<>();

    /** The total value of the UI. */
    /* package */ static final ReadableObjectPropertyKey<CharSequence> TOTAL =
            new ReadableObjectPropertyKey<>();

    /** The currency value of the UI. */
    /* package */ static final ReadableObjectPropertyKey<CharSequence> CURRENCY =
            new ReadableObjectPropertyKey<>();

    /** The callback when the continue button is pressed. */
    /* package */ static final ReadableObjectPropertyKey<Runnable> CONTINUE_BUTTON_CALLBACK =
            new ReadableObjectPropertyKey<>();

    /** The callback when the cancel button is pressed. */
    /* package */ static final ReadableObjectPropertyKey<Runnable> CANCEL_BUTTON_CALLBACK =
            new ReadableObjectPropertyKey<>();

    /* package */ static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {STORE_ORIGIN, PAYMENT_ICON, PAYMENT_INSTRUMENT_LABEL, TOTAL,
                    CURRENCY, CONTINUE_BUTTON_CALLBACK, CANCEL_BUTTON_CALLBACK};

    // Prevent instantiation.
    private SecurePaymentConfirmationAuthnProperties() {}
}
