// Copyright 2019 The Chromium Authors
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
     * @param errorReason The reason for the error, used internally to decide on specific failure
     * handling behavior.
     */
    default void onPaymentAppCreationError(
            String errorMessage, @AppCreationFailureReason int errorReason) {}

    /**
     * Called when the factory has finished creating all payment apps. A factory should call this
     * method exactly once.
     *
     * @param factory The factory that has finished creating all payment apps.
     */
    default void onDoneCreatingPaymentApps(PaymentAppFactoryInterface factory) {}

    /**
     * Forces canMakePayment() and hasEnrolledInstrument() to return true even when no payment
     * app is created.
     */
    default void setCanMakePaymentEvenWithoutApps() {}

    /**
     * Records that an Opt Out experience will be offered to the user in the
     * current UI flow.
     */
    default void setOptOutOffered() {}

    /** @return The Content-Security-Policy (CSP) checker. */
    CSPChecker getCSPChecker();
}
