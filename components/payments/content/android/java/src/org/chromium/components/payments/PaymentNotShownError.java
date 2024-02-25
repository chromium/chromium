// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.chromium.payments.mojom.PaymentErrorReason;

/** The error of payment UIs not being shown. */
public class PaymentNotShownError {
    private final String mErrorMessage;
    private final int mReason;

    /**
     * Creates an instance with the error details.
     *
     * @param errorMessage The error message for informing the web developer.
     * @param paymentErrorReason The reason of the payment error, defined in {@link
     *     PaymentErrorReason}.
     */
    /* package */ PaymentNotShownError(String errorMessage, int paymentErrorReason) {
        assert paymentErrorReason >= PaymentErrorReason.MIN_VALUE;
        assert paymentErrorReason <= PaymentErrorReason.MAX_VALUE;
        mErrorMessage = errorMessage;
        mReason = paymentErrorReason;
    }

    /** @return The error message for informing the web developer. */
    public String getErrorMessage() {
        return mErrorMessage;
    }

    /** @return The reason of the error, defined in {@link PaymentErrorReason}.*/
    public int getPaymentErrorReason() {
        return mReason;
    }
}
