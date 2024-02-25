// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.payments.mojom.PaymentAddress;

import java.nio.ByteBuffer;

/**
 * The interface for listener to payment method, shipping address, and shipping option change
 * events. Note: What the spec calls "payment methods" in the context of a "change event", this
 * code calls "apps".
 */
@JNINamespace("payments::android")
public interface PaymentRequestUpdateEventListener {
    /**
     * Called to notify merchant of payment method change. The payment app should block user
     * interaction until updateWith() or onPaymentDetailsNotUpdated().
     * https://w3c.github.io/payment-request/#paymentmethodchangeevent-interface
     *
     * @param methodName         Method name. For example, "https://google.com/pay". Should not
     *                           be null or empty.
     * @param stringifiedDetails JSON-serialized object. For example, {"type": "debit"}. Should
     *                           not be null.
     * @return Whether the payment state was valid.
     */
    @CalledByNative
    boolean changePaymentMethodFromInvokedApp(String methodName, String stringifiedDetails);

    /**
     * Called to notify merchant of shipping option change. The payment app should block user
     * interaction until updateWith() or onPaymentDetailsNotUpdated().
     * https://w3c.github.io/payment-request/#dom-paymentrequestupdateevent
     *
     * @param shippingOptionId Selected shipping option Identifier, Should not be null or
     *                         empty.
     * @return Whether the payment state was valid.
     */
    @CalledByNative
    boolean changeShippingOptionFromInvokedApp(String shippingOptionId);

    /**
     * Called to notify merchant of shipping address change. The payment app should block user
     * interaction until updateWith() or onPaymentDetailsNotUpdated().
     * https://w3c.github.io/payment-request/#dom-paymentrequestupdateevent
     *
     * @param shippingAddress Selected shipping address. Should not be null.
     * @return Whether the payment state was valid.
     */
    boolean changeShippingAddressFromInvokedApp(PaymentAddress shippingAddress);

    /**
     * Called to notify merchant of shipping address change. The payment app should block user
     * interaction until updateWith() or onPaymentDetailsNotUpdated().
     * https://w3c.github.io/payment-request/#dom-paymentrequestupdateevent
     *
     * @param shippingAddress Selected shipping address in serialized form. Should not be null.
     * @return Whether the payment state was valid.
     */
    @CalledByNative
    default boolean changeShippingAddress(ByteBuffer shippingAddress) {
        return changeShippingAddressFromInvokedApp(PaymentAddress.deserialize(shippingAddress));
    }
}
