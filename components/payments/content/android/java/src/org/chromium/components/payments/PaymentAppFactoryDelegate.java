// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

/**
 * Interface for providing information to a payment app factory and receiving the list of payment
 * apps.
 */
public interface PaymentAppFactoryDelegate {
    /** @return The information that a factory needs to create payment apps. */
    PaymentAppFactoryParams getParams();

    /**
     * Called when the "can make payment" value has been calculated. A factory should call this
     * method exactly once.
     *
     * @param canMakePayment Whether a payment app can support requested payment method.
     */
    default void onCanMakePaymentCalculated(boolean canMakePayment) {}

    /**
     * Called when a payment app factory has created a payment app.
     *
     * @param paymentApp A payment app.
     */
    void onPaymentAppCreated(PaymentApp paymentApp);

    /**
     * Called when a payment app factory has failed to create a payment app.
     *
     * @param errorMessage The error message for the web developer, e.g., "Failed to download the
     * web app manifest file."
     */
    default void onPaymentAppCreationError(String errorMessage) {}

    /**
     * Called when the factory has finished creating all payment apps. A factory should call this
     * method exactly once.
     *
     * @param factory The factory that has finished creating all payment apps.
     */
    default void onDoneCreatingPaymentApps(PaymentAppFactoryInterface factory) {}
}
