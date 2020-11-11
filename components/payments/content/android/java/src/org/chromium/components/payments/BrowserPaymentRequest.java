// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import androidx.annotation.Nullable;

import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentDetails;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.payments.mojom.PaymentOptions;
import org.chromium.payments.mojom.PaymentRequest;
import org.chromium.payments.mojom.PaymentResponse;
import org.chromium.payments.mojom.PaymentValidationErrors;
import org.chromium.url.GURL;

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
     * The client of the interface calls this when it has received the payment details update
     * from the merchant and has updated the PaymentRequestSpec.
     * @param details The details that the merchant provides to update the payment request.
     * @param hasNotifiedInvokedPaymentApp Whether the client has notified the invoked
     *      payment app of the updated details.
     */
    void onPaymentDetailsUpdated(PaymentDetails details, boolean hasNotifiedInvokedPaymentApp);

    /**
     * The browser part of the {@link PaymentRequest#onPaymentDetailsNotUpdated} implementation.
     * @param selectedShippingOptionError The selected shipping option error, can be null.
     */
    void onPaymentDetailsNotUpdated(@Nullable String selectedShippingOptionError);

    /** The browser part of the {@link PaymentRequest#complete} implementation. */
    void complete(int result);

    /**
     * The browser part of the {@link PaymentRequest#retry} implementation.
     * @param errors The merchant-defined error message strings, which are used to indicate to the
     *         end-user that something is wrong with the data of the payment response.
     */
    void retry(PaymentValidationErrors errors);

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
     * Shows the payment apps selector.
     * @return Whether the showing is successful.
     * @param isShowWaitingForUpdatedDetails Whether {@link PaymentRequest#show} is waiting for the
     *         updated details.
     */
    default boolean showAppSelector(boolean isShowWaitingForUpdatedDetails) {
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

    /**
     * Called when a new payment app is created.
     * @param paymentApp The new payment app.
     */
    default void onPaymentAppCreated(PaymentApp paymentApp) {}

    /**
     * @return Whether payment sheet based payment app is supported, e.g., user entering credit
     *      cards on payment sheet.
     */
    default boolean isPaymentSheetBasedPaymentAppSupported() {
        return false;
    }

    /**
     * Patches the given payment response if needed.
     * @param response The payment response to be patched in place.
     * @return Whether the patching is successful.
     */
    default boolean patchPaymentResponseIfNeeded(PaymentResponse response) {
        return true;
    }

    /**
     * Called by the payment app to let Chrome know that the payment app's UI is now hidden, but
     * the payment details have not been returned yet. This is a good time to show a "loading"
     * progress indicator UI.
     */
    default void onInstrumentDetailsLoading() {}

    /**
     * Called after retrieving payment details.
     */
    default void onInstrumentDetailsReady() {}

    /**
     * Called if unable to retrieve payment details.
     * @param errorMessage Developer-facing error message to be used when rejecting the promise
     *                     returned from PaymentRequest.show().
     */
    default void onInstrumentDetailsError(String errorMessage) {}

    /**
     * Opens a payment handler window and creates a WebContents with the given url to display in it.
     * @param url The url of the page to be opened in the window.
     * @return The created WebContents.
     */
    default WebContents openPaymentHandlerWindow(GURL url) {
        return null;
    }

    /** @return Whether any payment UI is being shown. */
    default boolean isShowingUi() {
        return false;
    }

    /**
     * Continues the unfinished part of show() that was blocked for the payment details that was
     * pending to be updated.
     */
    default void continueShow() {}

    /**
     * If needed, do extra parsing and validation for details.
     * @param details The details specified by the merchant.
     * @return True if the validation pass.
     */
    default boolean parseAndValidateDetailsFurtherIfNeeded(PaymentDetails details) {
        return true;
    }
}
