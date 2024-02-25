// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.chromium.payments.mojom.PaymentResponse;

/**
 * The interface of a helper class that generates a {@link PaymentResponse} with the input of
 * payment details.
 */
public interface PaymentResponseHelperInterface {
    /**
     * Generates a {@link PaymentResponse} with the given payment details.
     * @param methodName The payment method name being used for payment.
     * @param stringifiedDetails The payment details received from the payment app.
     * @param payerData The payer data received from the payment app.
     * @param resultCallback The callback that output the payment response.
     */
    void generatePaymentResponse(
            String methodName,
            String stringifiedDetails,
            PayerData payerData,
            PaymentResponseResultCallback resultCallback);

    /** The callback that output the payment response. */
    interface PaymentResponseResultCallback {
        /*
         * Called when the payment response is generated.
         * @param response The payment response to send to the merchant.
         */
        void onPaymentResponseReady(PaymentResponse response);
    }
}
