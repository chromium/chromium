// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentComplete;
import org.chromium.payments.mojom.PaymentDetails;
import org.chromium.payments.mojom.PaymentItem;
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
@NullMarked
public interface BrowserPaymentRequest {
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

    /**
     * Handles the payment request completion.
     * @param result The status of the transaction, defined in {@link PaymentComplete}.
     * @param onCompleteHandled Called when the complete has been handled.
     */
    void complete(int result, Runnable onCompleteHandled);

    /**
     * If retrying payments is supported, this method returns false.
     *
     * If retrying payments is not supported, this method disconnects the Mojo IPC pipe and returns
     * true.
     *
     * @return Whether the Mojo IPC pipe was disconnected due to lack of support for retrying
     * payments.
     */
    default boolean disconnectIfNoRetrySupport() {
        return false;
    }

    /**
     * Called when {@link PaymentRequest#retry} is invoked.
     * @param errors The merchant-defined error message strings, which are used to indicate to the
     *         end-user that something is wrong with the data of the payment response.
     */
    void onRetry(PaymentValidationErrors errors);

    /**
     * Close this instance. The callers of this method should stop referencing this instance upon
     * calling it. This method can be called within itself without causing infinite loop.
     */
    void close();

    /**
     * Performs extra validation for the given input and disconnects the mojo pipe if failed.
     *
     * @param webContents The WebContents that represents the merchant page.
     * @param methodData A map of the method data specified for the request.
     * @param details The payment details specified for the request.
     * @param paymentOptions The payment options specified for the request.
     * @return Whether this method has disconnected the mojo pipe.
     */
    default boolean disconnectIfExtraValidationFails(
            WebContents webContents,
            Map<String, PaymentMethodData> methodData,
            PaymentDetails details,
            PaymentOptions paymentOptions) {
        return false;
    }

    /**
     * Called when the PaymentRequestSpec is validated.
     * @param spec The validated PaymentRequestSpec.
     */
    void onSpecValidated(PaymentRequestSpec spec);

    /**
     * @return Whether at least one payment app (including basic-card payment app) is available
     *         (excluding the pending apps).
     */
    boolean hasAvailableApps();

    /**
     * Shows the payment apps selector or skip it to invoke the payment app directly.
     * @param isShowWaitingForUpdatedDetails Whether {@link PaymentRequest#show} is waiting for the
     *        updated details.
     * @param total The total amount specified in the payment request.
     * @param shouldSkipAppSelector True if the app selector should be skipped. Note that the
     *        implementer may consider other factors before deciding whether to show or skip.
     * @return The error of the showing if any; null if success.
     */
    @Nullable String showOrSkipAppSelector(
            boolean isShowWaitingForUpdatedDetails,
            @Nullable PaymentItem total,
            boolean shouldSkipAppSelector);

    /**
     * Notifies the payment UI service of the payment apps pending to be handled
     *
     * @param pendingApps The payment apps that are pending to be handled.
     */
    void notifyPaymentUiOfPendingApps(List<PaymentApp> pendingApps);

    /**
     * Called when the merchant requested Secure Payment Confirmation (SPC), but no credentials have
     * been found.
     *
     * @return Returns true if SPC is supported and enabled (e.g., in Chrome). Returns false if SPC
     *     is not supported or disabled (e.g., in WebView).
     */
    default boolean showNoMatchingPaymentCredential() {
        return false;
    }

    /**
     * Called when these conditions are satisfied: (1) show() has been called, (2) payment apps are
     * all queried, and (3) PaymentDetails is finalized.
     *
     * @return The error if it fails; null otherwise.
     */
    default @Nullable String onShowCalledAndAppsQueriedAndDetailsFinalized() {
        return null;
    }

    /**
     * Patches the given payment response if needed.
     * @param response The payment response to be patched in place.
     * @return Whether the patching is successful.
     */
    default boolean patchPaymentResponseIfNeeded(PaymentResponse response) {
        return true;
    }

    /** Called after retrieving payment details. */
    default void onInstrumentDetailsReady() {}

    /**
     * @return True if the app selector UI has been skipped. This method should not modify internal
     *         states.
     */
    default boolean hasSkippedAppSelector() {
        return true;
    }

    /**
     * Shows the app selector UI after the payment app invocation fails. This should be called
     * when the payment invocation fails and if the app selector was not skipped.
     */
    default void showAppSelectorAfterPaymentAppInvokeFailed() {}

    /**
     * Opens a payment handler window and creates a WebContents with the given url to display in it.
     *
     * @param url The url of the page to be opened in the window.
     * @param ukmSourceId The ukm source id assigned to the payment app.
     * @return The created WebContents.
     */
    default @Nullable WebContents openPaymentHandlerWindow(GURL url, long ukmSourceId) {
        return null;
    }

    /**
     * Continues the unfinished part of show() that was blocked for the payment details that was
     * pending to be updated.
     * @param details The updated payment details.
     * @param isFinishedQueryingPaymentApps Whether all payment app factories have been queried for
     *         their payment apps.
     * @return The error if it fails; null otherwise.
     */
    default @Nullable String continueShowWithUpdatedDetails(
            PaymentDetails details, boolean isFinishedQueryingPaymentApps) {
        return null;
    }

    /**
     * If needed, do extra parsing and validation for details.
     * @param details The details specified by the merchant.
     * @return True if the validation pass.
     */
    default boolean parseAndValidateDetailsFurtherIfNeeded(PaymentDetails details) {
        return true;
    }

    /** @return The selected payment app. */
    PaymentApp getSelectedPaymentApp();

    /** @return All of the available payment apps. */
    List<PaymentApp> getPaymentApps();

    /**
     * @return Whether the payment apps includes at least one that is "complete" which is defined
     *         by {@link PaymentApp#isComplete()}.
     */
    boolean hasAnyCompleteApp();

    /** @return Whether the shipping address section is visible. */
    default boolean isShippingSectionVisible() {
        return false;
    }

    /** @return Whether the contact info section is visible. */
    default boolean isContactSectionVisible() {
        return false;
    }

    /**
     * @return A dialog controller for displaying informational or warning messages.
     */
    DialogController getDialogController();

    /**
     * @return The site certificate chain of the web contents where PaymentRequest API was invoked.
     *     Can return null when ANDROID_PAYMENT_INTENTS_OMIT_DEPRECATED_PARAMETERS is enabled or
     *     when the page is localhost or is a file.
     */
    byte @Nullable [][] getCertificateChain();

    /**
     * @return The launcher for Android intent-based payment app.
     */
    AndroidIntentLauncher getAndroidIntentLauncher();

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
    boolean isFullDelegationRequired();

    /**
     * Send the given response to the renderer process to resolve the pending JavaScript promise for
     * the PaymentRequest.canMakePayment() API call, potentially overriding the calculated value.
     *
     * @param response The response to the JavaScript PaymentRequest.canMakePayment() API call. Can
     *     be potentially overridden.
     * @param sender The method for sending the response to the renderer process. May be invoked
     *     either synchronously or asynchronously.
     */
    default void maybeOverrideCanMakePaymentResponse(boolean response, Callback<Boolean> sender) {
        // By default, there is no override of the `response` value.
        sender.onResult(response);
    }

    /**
     * Send the given response to the renderer process to resolve the pending JavaScript promise for
     * the PaymentRequest.hasEnrolledInstrument() API call, potentially overriding the calculated
     * value.
     *
     * @param response The response to the JavaScript PaymentRequest.hasEnrolledInstrument() API
     *     call. Can be potentially overridden.
     * @param sender The method for sending the response to the renderer process. May be invoked
     *     either synchronously or asynchronously.
     */
    default void maybeOverrideHasEnrolledInstrumentResponse(
            boolean response, Callback<Boolean> sender) {
        // By default, there is no override of the `response` value.
        sender.onResult(response);
    }
}
