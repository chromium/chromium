// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Interface for providing information to a payment app factory and receiving the list of payment
 * apps.
 */
@NullMarked
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
     * Forces canMakePayment() and hasEnrolledInstrument() to return true even when no payment app
     * is created.
     */
    default void setCanMakePaymentEvenWithoutApps() {}

    /** Records that an Opt Out experience will be offered to the user in the current UI flow. */
    default void setOptOutOffered() {}

    /**
     * @return The Content-Security-Policy (CSP) checker.
     */
    CSPChecker getCSPChecker();

    /**
     * @return An instance of a dialog for displaying informational or warning messages.
     */
    default @Nullable DialogController getDialogController() {
        return null;
    }

    /**
     * @return The launcher for Android intent-based payment app.
     */
    default @Nullable AndroidIntentLauncher getAndroidIntentLauncher() {
        return null;
    }

    /**
     * Used to check whether payment apps are required to handle shipping address and contact
     * information, when merchant websites request that information. This information can be
     * returned either from payment apps or from Chrome's autofill. Result of this method does not
     * guarantee the payment. Even if this method returns true, there could be no payment apps to
     * support providing shipping address or contact information.
     *
     * @return Whether payment apps are required to provide shipping address and contact
     *     information.
     */
    default boolean isFullDelegationRequired() {
        return false;
    }
}
