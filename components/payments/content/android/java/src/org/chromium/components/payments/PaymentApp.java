// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import android.graphics.drawable.Drawable;

import androidx.annotation.Nullable;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.autofill.EditableOption;
import org.chromium.payments.mojom.PaymentDetailsModifier;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.payments.mojom.PaymentOptions;
import org.chromium.payments.mojom.PaymentRequestDetailsUpdate;
import org.chromium.payments.mojom.PaymentResponse;
import org.chromium.payments.mojom.PaymentShippingOption;

import java.util.List;
import java.util.Map;
import java.util.Set;

/** The base class for a single payment app, e.g., a payment handler. */
public abstract class PaymentApp extends EditableOption {
    /** Arbitrarily chosen maximum length of a payment app name. */
    private static final int APP_NAME_ELIDE_LENGTH = 64;

    /**
     * Whether complete and valid autofill data for merchant's request is available, e.g., if
     * merchant specifies `requestPayerEmail: true`, then this variable is true only if the autofill
     * data contains a valid email address. May be used in canMakePayment() for some types of
     * app, such as AutofillPaymentInstrument.
     */
    protected boolean mHaveRequestedAutofillData;

    /** The interface for the requester of payment details from the app. */
    public interface InstrumentDetailsCallback {
        /**
         * Called after retrieving payment details.
         *
         * @param methodName         Method name. For example, "visa".
         * @param stringifiedDetails JSON-serialized object. For example, {"card": "123"}.
         * @param payerData          Payer's shipping address and contact information.
         */
        void onInstrumentDetailsReady(
                String methodName, String stringifiedDetails, PayerData payerData);

        /**
         * Called if unable to retrieve payment details.
         * @param errorMessage Developer-facing error message to be used when rejecting the promise
         *                     returned from PaymentRequest.show().
         */
        void onInstrumentDetailsError(String errorMessage);
    }

    /** The interface for the requester to abort payment. */
    public interface AbortCallback {
        /**
         * Called after aborting payment is finished.
         *
         * @param abortSucceeded Indicates whether abort is succeed.
         */
        void onInstrumentAbortResult(boolean abortSucceeded);
    }

    protected PaymentApp(String id, String label, String sublabel, Drawable icon) {
        super(id, maybeElide(removeLineTerminators(label)), sublabel, icon);
    }

    protected PaymentApp(
            String id, String label, String sublabel, String tertiarylabel, Drawable icon) {
        super(id, maybeElide(removeLineTerminators(label)), sublabel, tertiarylabel, icon);
    }

    private static String removeLineTerminators(String text) {
        // '\n' - A newline (line feed) character.
        // '\f' - A form feed character.
        // '\r' - A carriage-return character.
        // '\u0085' - A next-line character.
        // '\u2028' - A line-separator character.
        // '\u2029' - A paragraph-separator character.
        // [abc] - a, b, or c (simple character class).
        // X+ - X, one or more times, a greedy quantifier.
        // See: https://docs.oracle.com/javase/7/docs/api/java/util/regex/Pattern.html
        return text.replaceAll("[\n\f\r\u0085\u2028\u2029]+", "");
    }

    private static String maybeElide(String text) {
        // 2026 is the unicode horizontal ellipsis.
        // See https://util.unicode.org/UnicodeJsps/character.jsp?a=2026.
        return text.length() <= APP_NAME_ELIDE_LENGTH
                ? text
                : text.substring(0, APP_NAME_ELIDE_LENGTH) + "\u2026";
    }

    /**
     * Sets the modified total for this payment app.
     *
     * @param modifiedTotal The new modified total to use.
     */
    public void setModifiedTotal(@Nullable String modifiedTotal) {
        updatePromoMessage(modifiedTotal);
    }

    /**
     * Returns a set of payment method names for this app, e.g., "basic-card".
     *
     * @return The method names for this app.
     */
    public abstract Set<String> getInstrumentMethodNames();

    /**
     * @return Whether this is a replacement for all server autofill apps. If at least one of
     *         the displayed apps returns true here, then all apps that return true in
     *         isServerAutofillInstrument() should be hidden.
     */
    public boolean isServerAutofillInstrumentReplacement() {
        return false;
    }

    /**
     * @return Whether the app supports the payment method with the method data. For example,
     *         supported card types and networks in the data should be verified for 'basic-card'
     *         payment method.
     */
    public boolean isValidForPaymentMethodData(String method, @Nullable PaymentMethodData data) {
        return getInstrumentMethodNames().contains(method);
    }

    /** @return Whether the app can collect and return shipping address. */
    public boolean handlesShippingAddress() {
        return false;
    }

    /** @return Whether the app can collect and return payer's name. */
    public boolean handlesPayerName() {
        return false;
    }

    /** @return Whether the app can collect and return payer's email. */
    public boolean handlesPayerEmail() {
        return false;
    }

    /** @return Whether the app can collect and return payer's phone. */
    public boolean handlesPayerPhone() {
        return false;
    }

    /**
     * @param haveRequestedAutofillData Whether complete and valid autofill data for merchant's
     *                                  request is available.
     */
    public void setHaveRequestedAutofillData(boolean haveRequestedAutofillData) {
        mHaveRequestedAutofillData = haveRequestedAutofillData;
    }

    /**
     * @return Whether this payment app should cause PaymentRequest.hasEnrolledInstrument() to
     *         return true.
     */
    public boolean hasEnrolledInstrument() {
        return true;
    }

    /** @return Whether this payment app can be pre-selected for immediate payment. */
    public boolean canPreselect() {
        return true;
    }

    /**
     * Invoke the payment app to retrieve the payment details.
     *
     * The callback will be invoked with the resulting payment details or error.
     *
     * @param id               The unique identifier of the PaymentRequest.
     * @param merchantName     The name of the merchant.
     * @param origin           The origin of this merchant.
     * @param iframeOrigin     The origin of the iframe that invoked PaymentRequest.
     * @param certificateChain The site certificate chain of the merchant. Can be null for localhost
     *                         or local file, which are secure contexts without SSL.
     * @param methodDataMap    The payment-method specific data for all applicable payment methods,
     *                         e.g., whether the app should be invoked in test or production, a
     *                         merchant identifier, or a public key.
     * @param total            The total amount.
     * @param displayItems     The shopping cart items.
     * @param modifiers        The relevant payment details modifiers.
     * @param paymentOptions   The payment options of the PaymentRequest.
     * @param shippingOptions  The shipping options of the PaymentRequest.
     * @param callback         The object that will receive the payment details.
     */
    public void invokePaymentApp(
            String id,
            String merchantName,
            String origin,
            String iframeOrigin,
            @Nullable byte[][] certificateChain,
            Map<String, PaymentMethodData> methodDataMap,
            PaymentItem total,
            List<PaymentItem> displayItems,
            Map<String, PaymentDetailsModifier> modifiers,
            PaymentOptions paymentOptions,
            List<PaymentShippingOption> shippingOptions,
            InstrumentDetailsCallback callback) {}

    /**
     * Update the payment information in response to payment method, shipping address, or shipping
     * option change events.
     *
     * @param response The merchant's response to the payment method, shipping address, or shipping
     *         option change events.
     */
    public void updateWith(PaymentRequestDetailsUpdate response) {}

    /**
     * Called when the merchant ignored the payment method, shipping address or shipping option
     * change event.
     */
    public void onPaymentDetailsNotUpdated() {}

    /**
     * @return True after changePaymentMethodFromInvokedApp(), changeShippingOptionFromInvokedApp(),
     *         or changeShippingAddressFromInvokedApp() and before update updateWith() or
     *         onPaymentDetailsNotUpdated().
     */
    public boolean isWaitingForPaymentDetailsUpdate() {
        return false;
    }

    /**
     * Abort invocation of the payment app.
     * @param callback The callback to return abort result.
     */
    public void abortPaymentApp(AbortCallback callback) {
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                new Runnable() {
                    @Override
                    public void run() {
                        callback.onInstrumentAbortResult(false);
                    }
                });
    }

    /** Cleans up any resources held by the payment app. For example, closes server connections. */
    public abstract void dismissInstrument();

    /**
     * @return The identifier for another payment app that should be hidden when this payment app is
     * present.
     */
    @Nullable
    public String getApplicationIdentifierToHide() {
        return null;
    }

    /**
     * @return The set of identifier of other apps that would cause this app to be hidden, if any of
     * them are present, e.g., ["com.bobpay.production", "com.bobpay.beta"].
     */
    @Nullable
    public Set<String> getApplicationIdentifiersThatHideThisApp() {
        return null;
    }

    /** @return The ukm source id assigned to the payment app. */
    public long getUkmSourceId() {
        return 0;
    }

    /**
     * Sets the endpoint for payment handler communication. Must be called before invoking this
     * payment app. Used only by payment apps that are backed by a payment handler.
     * @param host The endpoint for payment handler communication. Should not be null.
     */
    public void setPaymentHandlerHost(PaymentHandlerHost host) {}

    /** @return The type of payment app. */
    public @PaymentAppType int getPaymentAppType() {
        return PaymentAppType.UNDEFINED;
    }

    /**
     * @return Whether this app should be chosen over other available payment apps. For example,
     * when the Play Billing payment app is available in a TWA.
     */
    public boolean isPreferred() {
        return false;
    }

    /**
     * Updates the response IPC structure with the fields that are unique to this type of payment
     * app. Used when JSON serialization of payment method specific data is not being used. The
     * payment apps who need to set the fields should override this method.
     * @param response The PaymentResponse to whom the fields are set.
     * @return The PaymentResponse whose fields has been set.
     */
    public PaymentResponse setAppSpecificResponseFields(PaymentResponse response) {
        return response;
    }
}
