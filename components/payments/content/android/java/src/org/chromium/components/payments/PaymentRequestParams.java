// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.chromium.payments.mojom.PaymentDetailsModifier;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.payments.mojom.PaymentOptions;

import java.util.Map;

/** The parameters of PaymentRequest specified by the merchant. */
public interface PaymentRequestParams {
    /**
     * @return Whether or not the payment request is being aborted. Other methods should not get
     *         called when the payment request is being aborted.
     */
    boolean hasClosed();

    /** @return The PaymentOptions set by the merchant.  */
    PaymentOptions getPaymentOptions();

    /**
     * @return The unmodifiable mapping of method names to modifiers, which include modified totals
     * and additional line items. Used to display modified totals for each payment app, modified
     * total in order summary, and additional line items in order summary. Should not be null.
     */
    Map<String, PaymentDetailsModifier> getUnmodifiableModifiers();

    /**
     * @return The unmodifiable mapping of payment method identifier to the method-specific data in
     * the payment request.
     */
    Map<String, PaymentMethodData> getMethodData();

    /**
     * @return The raw total amount being charged - the total property of the PaymentDetails of
     * payment request.
     */
    PaymentItem getRawTotal();
}
