// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import android.os.Bundle;

import org.chromium.components.payments.IPaymentDetailsUpdateService;

/**
 * Helper interface used by the browser to notify the invoked native app about
 * merchant's response to one of the paymentmethodchange, shippingoptionchange,
 * or shippingaddresschange events.
 */
interface IPaymentDetailsUpdateServiceCallback {
    /**
     * Called to notify the invoked payment app about updated payment details
     * received from the merchant.
     *
     * @param updatedPaymentDetails The updated payment details received from
     *      the merchant.
     */
    oneway void updateWith(in Bundle updatedPaymentDetails);

    /**
     * Called to notify the invoked payment app that the merchant has not
     * modified the payment details.
     */
    oneway void paymentDetailsNotUpdated();

    /**
     * Called during a payment flow to point the payment app back to the payment
     * details update service to invoke when the user changes the payment
     * method, the shipping address, or the shipping option.
     *
     * @param service The payment details update service to invoke when the user
     *      changes the payment method, the shipping address, or the shipping
     *      option.
     */
    oneway void setPaymentDetailsUpdateService(
            IPaymentDetailsUpdateService service);
}
