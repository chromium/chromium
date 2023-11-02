// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.chromium.payments.mojom.PaymentErrorReason;

/** The error of payment UIs not being shown. */
public class PaymentNotShownError {
    private final int mNotShownReason;
    private final String mErrorMessage;
    private final int mReason;

    /**
     * Creates an instance with the error details.
     * @param notShownReason The reason of not showing UI, defined in {@link NotShownReason}.
     * @param errorMessage The error message for informing the web developer.
     * @param paymentErrorReason The reason of the payment error, defined in {@link
     *         PaymentErrorReason}.
     */
    /* package */ PaymentNotShownError(
            int notShownReason, String errorMessage, int paymentErrorReason) {
        assert notShownReason <= NotShownReason.MAX;
        assert paymentErrorReason >= PaymentErrorReason.MIN_VALUE;
        assert paymentErrorReason <= PaymentErrorReason.MAX_VALUE;
        mNotShownReason = notShownReason;
        mErrorMessage = errorMessage;
        mReason = paymentErrorReason;
    }

    /** @return The reason of not showing UI, defined in {@link NotShownReason}. */
    public int getNotShownReason() {
        return mNotShownReason;
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
