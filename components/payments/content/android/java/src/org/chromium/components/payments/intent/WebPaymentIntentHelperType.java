// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.intent;

import android.os.Bundle;
import android.os.Parcelable;
import android.text.TextUtils;
import android.util.JsonWriter;

import androidx.annotation.Nullable;

import java.io.IOException;
import java.io.StringWriter;
import java.util.Collection;
import java.util.List;

/**
 * The types that corresponds to the types in org.chromium.payments.mojom. The fields of these types
 * are the subset of those in the mojom types. The subset is minimally selected based on the need of
 * this package. This class should be independent of the org.chromium package.
 *
 * @see <a
 *         href="https://web.dev/android-payment-apps-overview/#parameters-2">Payment
 *         parameters</a>
 * @see <a
 *         href="https://web.dev/android-payment-apps-overview/#parameters">“Is
 *         ready to pay” parameters</a>
 */
public final class WebPaymentIntentHelperType {
    private static final String EMPTY_JSON_DATA = "{}";

    /** The class that corresponds to mojom.PaymentCurrencyAmount, with minimally required fields. */
    public static final class PaymentCurrencyAmount {
        public static String EXTRA_CURRENCY = "currency";
        public static String EXTRA_VALUE = "value";

        public final String currency;
        public final String value;

        public PaymentCurrencyAmount(String currency, String value) {
            this.currency = currency;
            this.value = value;
        }

        /**
         * Serializes this object into the provided json writer.
         * @param json The json object to which the seri
         */
        public void serialize(JsonWriter json) throws IOException {
            // {{{
            json.beginObject();
            json.name("currency").value(currency);
            json.name("value").value(value);
            json.endObject();
            // }}}
        }

        /**
         * Serializes this object
         * @return The serialized payment currency amount.
         */
        public String serialize() {
            StringWriter stringWriter = new StringWriter();
            JsonWriter json = new JsonWriter(stringWriter);
            try {
                serialize(json);
            } catch (IOException e) {
                return null;
            }
            return stringWriter.toString();
        }

        /* package */ Bundle asBundle() {
            Bundle bundle = new Bundle();
            bundle.putString(EXTRA_CURRENCY, currency);
            bundle.putString(EXTRA_VALUE, value);
            return bundle;
        }
    }

    /** The class that corresponds mojom.PaymentItem, with minimally required fields. */
    public static final class PaymentItem {
        public final PaymentCurrencyAmount amount;

        public PaymentItem(PaymentCurrencyAmount amount) {
            this.amount = amount;
        }

        /**
         * Serializes this object into the provided json writer after adding an empty string for the
         * redacted "label" field.
         * @param  json  The json writer used for serialization
         */
        public void serializeAndRedact(JsonWriter json) throws IOException {
            // item {{{
            json.beginObject();
            // Redact the total label, because the payment app does not need it to complete the
            // transaction. Matches the behavior of:
            // https://w3c.github.io/payment-handler/#total-attribute
            json.name("label").value("");

            // amount {{{
            json.name("amount");
            amount.serialize(json);
            // }}} amount

            json.endObject();
            // }}} item
        }
    }

    /** The class that corresponds mojom.PaymentDetailsModifier, with minimally required fields. */
    public static final class PaymentDetailsModifier {
        public final PaymentItem total;
        public final PaymentMethodData methodData;

        public PaymentDetailsModifier(PaymentItem total, PaymentMethodData methodData) {
            this.total = total;
            this.methodData = methodData;
        }

        /**
         * Serializes payment details modifiers.
         * @param  modifiers The collection of details modifiers to serialize.
         * @return The serialized payment details modifiers
         */
        public static String serializeModifiers(Collection<PaymentDetailsModifier> modifiers) {
            StringWriter stringWriter = new StringWriter();
            JsonWriter json = new JsonWriter(stringWriter);
            try {
                json.beginArray();
                for (PaymentDetailsModifier modifier : modifiers) {
                    checkNotNull(modifier, "PaymentDetailsModifier");
                    modifier.serialize(json);
                }
                json.endArray();
            } catch (IOException e) {
                return EMPTY_JSON_DATA;
            }
            return stringWriter.toString();
        }

        private void serialize(JsonWriter json) throws IOException {
            // {{{
            json.beginObject();

            // total {{{
            if (total != null) {
                json.name("total");
                total.serializeAndRedact(json);
            } else {
                json.name("total").nullValue();
            }
            // }}} total

            // TODO(crbug.com/41338971): The supportedMethods field was already changed from
            // array to string but we should keep backward-compatibility for now. supportedMethods
            // {{{
            json.name("supportedMethods").beginArray();
            json.value(methodData.supportedMethod);
            json.endArray();
            // }}} supportedMethods

            // data {{{
            json.name("data").value(methodData.stringifiedData);
            // }}}

            json.endObject();
            // }}}
        }
    }

    /** The class that corresponds mojom.PaymentMethodData, with minimally required fields. */
    public static final class PaymentMethodData {
        public final String supportedMethod;
        public final String stringifiedData;

        public PaymentMethodData(String supportedMethod, String stringifiedData) {
            this.supportedMethod = supportedMethod;
            this.stringifiedData = stringifiedData;
        }
    }

    /** The class that mirrors mojom.PaymentShippingOption. */
    public static final class PaymentShippingOption {
        public static final String EXTRA_SHIPPING_OPTION_ID = "id";
        public static final String EXTRA_SHIPPING_OPTION_LABEL = "label";
        public static final String EXTRA_SHIPPING_OPTION_AMOUNT = "amount";
        public static final String EXTRA_SHIPPING_OPTION_SELECTED = "selected";

        public final String id;
        public final String label;
        public final PaymentCurrencyAmount amount;
        public final boolean selected;

        public PaymentShippingOption(
                String id, String label, PaymentCurrencyAmount amount, boolean selected) {
            this.id = id;
            this.label = label;
            this.amount = amount;
            this.selected = selected;
        }

        private Bundle asBundle() {
            Bundle bundle = new Bundle();
            bundle.putString(EXTRA_SHIPPING_OPTION_ID, id);
            bundle.putString(EXTRA_SHIPPING_OPTION_LABEL, label);
            bundle.putBundle(EXTRA_SHIPPING_OPTION_AMOUNT, amount.asBundle());
            bundle.putBoolean(EXTRA_SHIPPING_OPTION_SELECTED, selected);
            return bundle;
        }

        /**
         * Create a parcelable array of payment shipping options.
         * @param  shippingOptions The list of available shipping options
         * @return The parcelable array of shipping options passed to the native payment app.
         */
        public static Parcelable[] buildPaymentShippingOptionList(
                List<PaymentShippingOption> shippingOptions) {
            Parcelable[] result = new Parcelable[shippingOptions.size()];
            int index = 0;
            for (PaymentShippingOption option : shippingOptions) {
                result[index++] = option.asBundle();
            }
            return result;
        }
    }

    /** The class that mirrors mojom.PaymentOptions. */
    public static final class PaymentOptions {
        public final boolean requestPayerName;
        public final boolean requestPayerEmail;
        public final boolean requestPayerPhone;
        public final boolean requestShipping;
        public final String shippingType;

        public PaymentOptions(
                boolean requestPayerName,
                boolean requestPayerEmail,
                boolean requestPayerPhone,
                boolean requestShipping,
                @Nullable String shippingType) {
            this.requestPayerName = requestPayerName;
            this.requestPayerEmail = requestPayerEmail;
            this.requestPayerPhone = requestPayerPhone;
            this.requestShipping = requestShipping;
            this.shippingType = shippingType;
        }
    }

    private static void checkNotNull(Object value, String name) {
        if (value == null) throw new IllegalArgumentException(name + " should not be null.");
    }

    /** The class that mirrors mojom.PaymentHandlerMethodData. */
    public static final class PaymentHandlerMethodData {
        public static final String EXTRA_METHOD_NAME = "methodName";
        public static final String EXTRA_STRINGIFIED_DETAILS = "details";

        public final String methodName;
        public final String stringifiedData;

        public PaymentHandlerMethodData(String methodName, String stringifiedData) {
            this.methodName = methodName;
            this.stringifiedData = stringifiedData;
        }

        /* package */ Bundle asBundle() {
            Bundle bundle = new Bundle();
            bundle.putString(EXTRA_METHOD_NAME, methodName);
            bundle.putString(EXTRA_STRINGIFIED_DETAILS, stringifiedData);
            return bundle;
        }
    }

    /** The class that mirrors mojom.PaymentRequestDetailsUpdate. */
    public static final class PaymentRequestDetailsUpdate {
        public static final String EXTRA_TOTAL = "total";
        public static final String EXTRA_SHIPPING_OPTIONS = "shippingOptions";
        public static final String EXTRA_ERROR_MESSAGE = "error";
        public static final String EXTRA_STRINGIFIED_PAYMENT_METHOD_ERRORS =
                "stringifiedPaymentMethodErrors";
        public static final String EXTRA_ADDRESS_ERRORS = "addressErrors";

        @Nullable public final PaymentCurrencyAmount total;
        @Nullable public final List<PaymentShippingOption> shippingOptions;
        @Nullable public final String error;
        @Nullable public final String stringifiedPaymentMethodErrors;
        @Nullable public final Bundle bundledShippingAddressErrors;

        public PaymentRequestDetailsUpdate(
                @Nullable PaymentCurrencyAmount total,
                @Nullable List<PaymentShippingOption> shippingOptions,
                @Nullable String error,
                @Nullable String stringifiedPaymentMethodErrors,
                @Nullable Bundle bundledShippingAddressErrors) {
            this.total = total;
            this.shippingOptions = shippingOptions;
            this.error = error;
            this.stringifiedPaymentMethodErrors = stringifiedPaymentMethodErrors;
            this.bundledShippingAddressErrors = bundledShippingAddressErrors;
        }

        /**
         * Converts PaymentRequestDetailsUpdate to a bundle which will be passed to the invoked
         * payment app.
         * @return The converted PaymentRequestDetailsUpdate
         */
        public Bundle asBundle() {
            Bundle bundle = new Bundle();
            if (total != null) {
                bundle.putBundle(WebPaymentIntentHelper.EXTRA_TOTAL, total.asBundle());
            }
            if (shippingOptions != null && !shippingOptions.isEmpty()) {
                bundle.putParcelableArray(
                        EXTRA_SHIPPING_OPTIONS,
                        PaymentShippingOption.buildPaymentShippingOptionList(shippingOptions));
            }
            if (!TextUtils.isEmpty(error)) bundle.putString(EXTRA_ERROR_MESSAGE, error);
            if (!TextUtils.isEmpty(stringifiedPaymentMethodErrors)) {
                bundle.putString(
                        EXTRA_STRINGIFIED_PAYMENT_METHOD_ERRORS, stringifiedPaymentMethodErrors);
            }
            if (bundledShippingAddressErrors != null) {
                bundle.putBundle(EXTRA_ADDRESS_ERRORS, bundledShippingAddressErrors);
            }
            return bundle;
        }
    }
}
