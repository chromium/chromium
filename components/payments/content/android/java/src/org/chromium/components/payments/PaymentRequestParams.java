// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.chromium.build.annotations.MockedInTests;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.payments.mojom.PaymentDetailsModifier;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.payments.mojom.PaymentOptions;

import java.util.Map;

/** The parameters of PaymentRequest specified by the merchant. */
@MockedInTests // Needed due to R8's computeDelayedInterfaceMethodSyntheticBridges. b/147584922
@NullMarked
public interface PaymentRequestParams {
    /**
     * Returns whether the payment request has closed. Other methods should not get called when the
     * payment request has closed.
     */
    boolean hasClosed();

    /** Returns the PaymentOptions set by the merchant. */
    PaymentOptions getPaymentOptions();

    /**
     * Returns the unmodifiable mapping of method names to modifiers, which include modified totals
     * and additional line items. Used to display modified totals for each payment app, the modified
     * total in the order summary, and additional line items in the order summary. Should not be
     * null.
     */
    Map<String, PaymentDetailsModifier> getUnmodifiableModifiers();

    /**
     * Returns the unmodifiable mapping of payment method identifier to the method-specific data in
     * the payment request.
     */
    Map<String, PaymentMethodData> getMethodData();

    /**
     * Returns the raw total amount being charged - the total property of the PaymentDetails of the
     * payment request.
     */
    @Nullable PaymentItem getRawTotal();
}
