// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.chromium.build.annotations.NullMarked;

import java.util.List;

/** Interface for clients of PaymentAppService. */
@NullMarked
public interface PaymentAppServiceDelegate {
    /**
     * @return The information that a factory needs to create payment apps.
     */
    PaymentAppFactoryParams getParams();

    /**
     * Called when the "can make payment" value has been calculated.
     *
     * @param canMakePayment Whether a payment app can support requested payment method.
     */
    void onCanMakePaymentCalculated(boolean canMakePayment);

    /**
     * Called when a payment app factory has failed to create a payment app.
     *
     * @param errorMessage The error message for the web developer.
     * @param errorReason The reason for the error.
     */
    void onPaymentAppCreationError(String errorMessage, @AppCreationFailureReason int errorReason);

    /**
     * Called when the service has finished creating all payment apps.
     *
     * @param apps The list of apps that were created by all factories owned by the service.
     */
    void onDoneCreatingPaymentApps(List<PaymentApp> apps);

    /**
     * Forces canMakePayment() and hasEnrolledInstrument() to return true even when no payment app
     * is created.
     */
    void setCanMakePaymentEvenWithoutApps();

    /** Records that an Opt Out experience will be offered to the user in the current UI flow. */
    void setOptOutOffered();
}
