// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.chromium.payments.mojom.PaymentDetails;
import org.chromium.payments.mojom.PaymentErrorReason;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.payments.mojom.PaymentRequest;
import org.chromium.payments.mojom.PaymentValidationErrors;

/**
 * The browser part of the PaymentRequest implementation. The browser here can be either the
 * Android Chrome browser or the WebLayer "browser".
 */
public interface BrowserPaymentRequest {
    /** The factory that creates an instance of {@link BrowserPaymentRequest}. */
    interface Factory {
        /**
         * Create an instance of {@link BrowserPaymentRequest}.
         * @param componentPaymentRequestImpl The ComponentPaymentRequestImpl to work together with
         *         the BrowserPaymentRequest instance, cannot be null.
         * @return An instance of BrowserPaymentRequest, cannot be null.
         */
        BrowserPaymentRequest createBrowserPaymentRequest(
                ComponentPaymentRequestImpl componentPaymentRequestImpl);
    }

    /**
     * Initialize the browser part of the {@link PaymentRequest} implementation and validate the raw
     * payment request data coming from the untrusted mojo.
     * @param methodData The supported methods specified by the merchant, cannot be null.
     * @param details The payment details specified by the merchant, cannot be null.
     * @param googlePayBridgeEligible True when the renderer process deems the current request
     *         eligible for the skip-to-GPay experimental flow. It is ultimately up to the browser
     *         process to determine whether to trigger it
     * @return whether the initialization is successful.
     */
    boolean initAndValidate(PaymentMethodData[] methodData, PaymentDetails details,
            boolean googlePayBridgeEligible);

    /**
     * The browser part of the {@link PaymentRequest#show} implementation.
     * @param isUserGesture Whether this method is triggered from a user gesture.
     * @param waitForUpdatedDetails Whether to wait for updated details. It's true when merchant
     *         passed in a promise into PaymentRequest.show(), so Chrome should disregard the
     *         initial payment details and show a spinner until the promise resolves with the
     *         correct payment details.
     */
    void show(boolean isUserGesture, boolean waitForUpdatedDetails);

    /**
     * The browser part of the {@link PaymentRequest#updateWith} implementation.
     * @param details The details that the merchant provides to update the payment request.
     */
    void updateWith(PaymentDetails details);

    /** The browser part of the {@link PaymentRequest#onPaymentDetailsNotUpdated} implementation. */
    void onPaymentDetailsNotUpdated();

    /** The browser part of the {@link PaymentRequest#abort} implementation. */
    void abort();

    /** The browser part of the {@link PaymentRequest#complete} implementation. */
    void complete(int result);

    /**
     * The browser part of the {@link PaymentRequest#retry} implementation.
     * @param errors The merchant-defined error message strings, which are used to indicate to the
     *         end-user that something is wrong with the data of the payment response.
     */
    void retry(PaymentValidationErrors errors);

    /**
     * The browser part of the {@link PaymentRequest#hasEnrolledInstrument} implementation.
     */
    void hasEnrolledInstrument();

    /** The browser part of the {@link PaymentRequest#canMakePayment} implementation. */
    void canMakePayment();

    /**
     * Delegate to the same method of PaymentRequestImpl.
     * @param debugMessage The debug message shown for web developers.
     * @param reason The reason of the disconnection defined in {@link PaymentErrorReason}.
     */
    void disconnectFromClientWithDebugMessage(String debugMessage, int reason);

    /**
     * Close this instance. The callers of this method should stop referencing this instance upon
     * calling it. This method can be called within itself without causing infinite loop.
     */
    void close();
}
