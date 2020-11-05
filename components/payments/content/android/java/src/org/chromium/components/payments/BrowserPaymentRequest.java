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

import java.util.List;
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

    default void onWhetherGooglePayBridgeEligible(boolean googlePayBridgeEligible,
            WebContents webContents, PaymentMethodData[] rawMethodData) {}

    /**
     * @return Whether at least one payment app (including basic-card payment app) is available
     *         (excluding the pending apps).
     */
    default boolean hasAvailableApps() {
        return false;
    }

    /**
     * If strict show() conditions are not satisfied, disconnect from client and return true.
     * @param isUserGestureShow Whether the PaymentRequest.show() is triggered by user gesture.
     * @return Whether client has been disconnected.
     */
    default boolean disconnectForStrictShow(boolean isUserGestureShow) {
        return false;
    }

    /**
     * Shows the payment apps selector.
     * @return Whether the showing is successful.
     */
    default boolean showAppSelector() {
        return false;
    }

    /**
     * Notifies the payment UI service of the payment apps pending to be handled
     * @param pendingApps The payment apps that are pending to be handled.
     */
    default void notifyPaymentUiOfPendingApps(List<PaymentApp> pendingApps) {}

    /**
     * Skips the app selector UI whether it is currently opened or not, and if applicable, invokes a
     * payment app.
     */
    default void triggerPaymentAppUiSkipIfApplicable() {}

    /** @return The error message of rejecting the show() request. */
    default String getRejectShowErrorMessage() {
        return "";
    }

    /**
     * Called when a new payment app is created.
     * @param paymentApp The new payment app.
     */
    default void onPaymentAppCreated(PaymentApp paymentApp) {}

    // TODO(crbug.com/1144527): this method will be removed once PaymentRequestService has taken
    // over PaymentRequestUpdateEventListener.
    /** @return An instance of PaymentRequestUpdateEventListener. */
    default PaymentRequestUpdateEventListener getPaymentRequestUpdateEventListener() {
        return null;
    }

    /**
     * @return Whether payment sheet based payment app is supported, e.g., user entering credit
     *      cards on payment sheet.
     */
    default boolean isPaymentSheetBasedPaymentAppSupported() {
        return false;
    }

    // TODO(crbug.com/1144527): this method will be removed once PaymentRequestService has taken
    // over mRejectShowErrorMessage.
    /**
     * Set the error message for show rejection.
     * @param errorMessage The error message for show rejection.
     */
    default void setRejectShowErrorMessage(String errorMessage) {}
}
