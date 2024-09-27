// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.payments.mojom.PaymentDetails;
import org.chromium.payments.mojom.PaymentValidationErrors;

import java.nio.ByteBuffer;

/** Static class to represent a JNI interface to a C++ validation library. */
@JNINamespace("payments")
public class PaymentValidator {
    public static boolean validatePaymentDetails(PaymentDetails details) {
        if (details == null) {
            return false;
        }
        return PaymentValidatorJni.get().validatePaymentDetailsAndroid(details.serialize());
    }

    public static boolean validatePaymentValidationErrors(PaymentValidationErrors errors) {
        if (errors == null) {
            return false;
        }
        return PaymentValidatorJni.get().validatePaymentValidationErrorsAndroid(errors.serialize());
    }

    @NativeMethods
    interface Natives {
        boolean validatePaymentDetailsAndroid(ByteBuffer buffer);

        boolean validatePaymentValidationErrorsAndroid(ByteBuffer buffer);
    }
}
