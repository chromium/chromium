// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import android.os.Bundle;

import org.chromium.components.payments.IPaymentDetailsUpdateServiceCallback;

/**
 * Helper interface used by the invoked native payment app to notify the
 * browser that the user has selected a different payment method, shipping
 * option, or shipping address.
 */
interface IPaymentDetailsUpdateService {
    /**
     * Called to notify the browser that the user has selected a different
     * payment method.
     *
     * @param paymentHandlerMethodData The data containing the selected payment
     *      method's name and optional stringified details.
     */
    oneway void changePaymentMethod(in Bundle paymentHandlerMethodData,
            IPaymentDetailsUpdateServiceCallback callback);

    /**
     * Called to notify the browser that the user has selected a different
     * shipping option.
     *
     * @param shippingOptionId The identifier of the selected shipping option.
     */
    oneway void changeShippingOption(in String shippingOptionId,
            IPaymentDetailsUpdateServiceCallback callback);

    /**
     * Called to notify the browser that the user has selected a different
     * shipping address.
     *
     * @param shippingAddress The selected shipping address.
     */
    oneway void changeShippingAddress(in Bundle shippingAddress,
            IPaymentDetailsUpdateServiceCallback callback);
}
