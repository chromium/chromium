// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import android.os.Bundle;

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
}
