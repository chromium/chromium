// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.chromium.payments.mojom.PaymentDetails;
import org.chromium.payments.mojom.PaymentHandlerMethodData;
import org.chromium.payments.mojom.PaymentHandlerModifier;
import org.chromium.payments.mojom.PaymentRequestDetailsUpdate;
import org.chromium.payments.mojom.PaymentShippingOption;

import java.util.ArrayList;

/**
 * Redacts and converts the payment details update from the merchant into a data structure to be
 * sent to the invoked payment handler.
 */
public class PaymentDetailsConverter {
    /**
     * To be implemented by the object that can check whether the invoked payment instrument is
     * valid for the given payment method identifier.
     */
    public interface MethodChecker {
        /**
         * Checks whether the invoked payment instrument is valid for the given payment method
         * identifier.
         * @param methodName Payment method identifier.
         * @param invokedPaymentApp The invoked payment instrument (app).
         * @return Whether the invoked instrument is valid for the given payment method identifier.
         */
        boolean isInvokedInstrumentValidForPaymentMethodIdentifier(
                String methodName, PaymentApp invokedPaymentApp);
    }

    /** This class has only static methods. */
    private PaymentDetailsConverter() {}

    /**
     * Redacts and converts the payment details update from the merchant into a data structure to be
     * sent to the payment handler.
     * @param details       The pre-validated payment details update from the merchant. Should not
     *                      be null.
     * @param methodChecker The object that can check whether the invoked payment instrument is
     *                      valid for the given payment method identifier. Should not be null.
     * @param invokedPaymentApp The invoked payment app.
     * @return The data structure that can be sent to the invoked payment handler.
     */
    public static PaymentRequestDetailsUpdate convertToPaymentRequestDetailsUpdate(
            PaymentDetails details, MethodChecker methodChecker, PaymentApp invokedPaymentApp) {
        // Keep in sync with components/payments/content/payment_details_converter.cc.
        assert details != null;
        assert methodChecker != null;

        boolean handlesShipping = invokedPaymentApp.handlesShippingAddress();
        PaymentRequestDetailsUpdate response = new PaymentRequestDetailsUpdate();
        response.error = details.error;
        response.stringifiedPaymentMethodErrors = details.stringifiedPaymentMethodErrors;
        if (handlesShipping) response.shippingAddressErrors = details.shippingAddressErrors;

        if (details.total != null) response.total = details.total.amount;

        if (details.modifiers != null) {
            ArrayList<PaymentHandlerModifier> modifiers = new ArrayList<>();

            for (int i = 0; i < details.modifiers.length; i++) {
                if (!methodChecker.isInvokedInstrumentValidForPaymentMethodIdentifier(
                        details.modifiers[i].methodData.supportedMethod, invokedPaymentApp)) {
                    continue;
                }

                PaymentHandlerModifier modifier = new PaymentHandlerModifier();
                modifier.methodData = new PaymentHandlerMethodData();
                modifier.methodData.methodName = details.modifiers[i].methodData.supportedMethod;
                modifier.methodData.stringifiedData =
                        details.modifiers[i].methodData.stringifiedData;

                if (details.modifiers[i].total != null) {
                    modifier.total = details.modifiers[i].total.amount;
                }

                modifiers.add(modifier);
            }

            response.modifiers = modifiers.toArray(new PaymentHandlerModifier[modifiers.size()]);
        }

        if (handlesShipping && details.shippingOptions != null) {
            ArrayList<PaymentShippingOption> options = new ArrayList<>();
            for (int i = 0; i < details.shippingOptions.length; i++) {
                PaymentShippingOption option = new PaymentShippingOption();
                option.amount = details.shippingOptions[i].amount;
                option.id = details.shippingOptions[i].id;
                option.label = details.shippingOptions[i].label;
                option.selected = details.shippingOptions[i].selected;

                options.add(option);
            }

            response.shippingOptions = options.toArray(new PaymentShippingOption[options.size()]);
        }

        return response;
    }
}
