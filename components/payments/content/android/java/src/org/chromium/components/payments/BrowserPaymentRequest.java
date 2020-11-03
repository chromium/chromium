// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import androidx.annotation.Nullable;

import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentDetails;
import org.chromium.payments.mojom.PaymentErrorReason;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.payments.mojom.PaymentOptions;
import org.chromium.payments.mojom.PaymentRequest;
import org.chromium.payments.mojom.PaymentValidationErrors;

import java.util.Map;

/**
 * The browser part of the PaymentRequest implementation. The browser here can be either the
 * Android Chrome browser or the WebLayer "browser".
 */
public interface BrowserPaymentRequest {
    /** The factory that creates an instance of {@link BrowserPaymentRequest}. */
    interface Factory {
        /**
         * Create an instance of {@link BrowserPaymentRequest}.
         * @param paymentRequestService The PaymentRequestService to work together with
         *         the BrowserPaymentRequest instance, cannot be null.
         * @return An instance of BrowserPaymentRequest, cannot be null.
         */
        BrowserPaymentRequest createBrowserPaymentRequest(
                PaymentRequestService paymentRequestService);
    }

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
     * Delegate to the same method of ChromePaymentRequestService.
     * @param debugMessage The debug message shown for web developers.
     * @param reason The reason of the disconnection defined in {@link PaymentErrorReason}.
     */
    void disconnectFromClientWithDebugMessage(String debugMessage, int reason);

    /**
     * Close this instance. The callers of this method should stop referencing this instance upon
     * calling it. This method can be called within itself without causing infinite loop.
     */
    void close();

    /**
     * Modifies the given method data.
     * @param methodData A map of method names to PaymentMethodData, could be null. This parameter
     * could be modified in place.
     */
    default void modifyMethodData(@Nullable Map<String, PaymentMethodData> methodData) {}

    /**
     * Called when queryForQuota is created.
     * @param queryForQuota The created queryForQuota, which could be modified in place.
     */
    default void onQueryForQuotaCreated(Map<String, PaymentMethodData> queryForQuota) {}

    /**
     * Performs extra validation for the given input and disconnects the mojo pipe if failed.
     * @param webContents The WebContents that represents the merchant page.
     * @param methodData A map of the method data specified for the request.
     * @param details The payment details specified for the request.
     * @param paymentOptions The payment options specified for the request.
     * @return Whether this method has disconnected the mojo pipe.
     */
    default boolean disconnectIfExtraValidationFails(WebContents webContents,
            Map<String, PaymentMethodData> methodData, PaymentDetails details,
            PaymentOptions paymentOptions) {
        return false;
    }

    /**
     * Called when the PaymentRequestSpec is validated.
     * @param spec The validated PaymentRequestSpec.
     */
    default void onSpecValidated(PaymentRequestSpec spec) {}

    /**
     * Adds the PaymentAppFactory(s) specified by the implementers to the given PaymentAppService.
     * @param service The PaymentAppService to be added with the factories.
     */
    void addPaymentAppFactories(PaymentAppService service);

    /** @return A PaymentAppFactoryDelegate to be used with the PaymentAppService. */
    PaymentAppFactoryDelegate getPaymentAppFactoryDelegate();

    default void onWhetherGooglePayBridgeEligible(boolean googlePayBridgeEligible,
            WebContents webContents, PaymentMethodData[] rawMethodData) {}
}
