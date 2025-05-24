// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.intent;

import android.os.Bundle;
import android.text.TextUtils;

import org.chromium.build.annotations.Contract;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.OptimizeAsNonNull;
import org.chromium.payments.mojom.AddressErrors;
import org.chromium.payments.mojom.PaymentCurrencyAmount;
import org.chromium.payments.mojom.PaymentDetailsModifier;
import org.chromium.payments.mojom.PaymentHandlerMethodData;
import org.chromium.payments.mojom.PaymentHandlerModifier;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.payments.mojom.PaymentOptions;
import org.chromium.payments.mojom.PaymentRequestDetailsUpdate;
import org.chromium.payments.mojom.PaymentShippingOption;
import org.chromium.payments.mojom.PaymentShippingType;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * This class defines the utility functions that convert the payment info types in
 * org.chromium.payments.mojom to their counterparts in WebPaymentIntentHelperType.
 */
@NullMarked
public final class WebPaymentIntentHelperTypeConverter {

    @Contract("!null -> !null")
    public static WebPaymentIntentHelperType.@Nullable PaymentCurrencyAmount
            fromMojoPaymentCurrencyAmount(@Nullable PaymentCurrencyAmount currencyAmount) {
        if (currencyAmount == null) return null;
        return new WebPaymentIntentHelperType.PaymentCurrencyAmount(
                /* currency= */ currencyAmount.currency, /* value= */ currencyAmount.value);
    }

    @Contract("!null -> !null")
    public static WebPaymentIntentHelperType.@Nullable PaymentItem fromMojoPaymentItem(
            @Nullable PaymentItem item) {
        if (item == null) return null;
        return new WebPaymentIntentHelperType.PaymentItem(
                fromMojoPaymentCurrencyAmount(item.amount));
    }

    @Contract("!null -> !null")
    public static WebPaymentIntentHelperType.@Nullable PaymentDetailsModifier
            fromMojoPaymentDetailsModifier(@Nullable PaymentDetailsModifier detailsModifier) {
        if (detailsModifier == null) return null;
        return new WebPaymentIntentHelperType.PaymentDetailsModifier(
                fromMojoPaymentItem(detailsModifier.total),
                fromMojoPaymentMethodData(detailsModifier.methodData));
    }

    @Contract("!null -> !null")
    public static WebPaymentIntentHelperType.@Nullable PaymentMethodData fromMojoPaymentMethodData(
            @Nullable PaymentMethodData methodData) {
        if (methodData == null) return null;
        return new WebPaymentIntentHelperType.PaymentMethodData(
                /* supportedMethod= */ methodData.supportedMethod,
                /* stringifiedData= */ methodData.stringifiedData);
    }

    @Contract("!null -> !null")
    public static @Nullable
            Map<String, WebPaymentIntentHelperType.PaymentMethodData> fromMojoPaymentMethodDataMap(
                    @Nullable Map<String, PaymentMethodData> methodDataMap) {
        if (methodDataMap == null) return null;
        Map<String, WebPaymentIntentHelperType.PaymentMethodData> compatibleMethodDataMap =
                new HashMap<>();
        for (var entry : methodDataMap.entrySet()) {
            compatibleMethodDataMap.put(
                    entry.getKey(),
                    WebPaymentIntentHelperTypeConverter.fromMojoPaymentMethodData(
                            entry.getValue()));
        }
        return compatibleMethodDataMap;
    }

    @Contract("!null -> !null")
    public static @Nullable
            Map<String, WebPaymentIntentHelperType.PaymentDetailsModifier>
                    fromMojoPaymentDetailsModifierMap(
                            @Nullable Map<String, PaymentDetailsModifier> modifiers) {
        if (modifiers == null) return null;
        Map<String, WebPaymentIntentHelperType.PaymentDetailsModifier> compatibleModifiers =
                new HashMap<>();
        for (var entry : modifiers.entrySet()) {
            compatibleModifiers.put(
                    entry.getKey(),
                    WebPaymentIntentHelperTypeConverter.fromMojoPaymentDetailsModifier(
                            entry.getValue()));
        }
        return compatibleModifiers;
    }

    @Contract("!null -> !null")
    public static @Nullable List<WebPaymentIntentHelperType.PaymentItem> fromMojoPaymentItems(
            @Nullable List<PaymentItem> paymentItems) {
        if (paymentItems == null) return null;
        List<WebPaymentIntentHelperType.PaymentItem> compatiblePaymentItems = new ArrayList<>();
        for (var element : paymentItems) {
            compatiblePaymentItems.add(
                    WebPaymentIntentHelperTypeConverter.fromMojoPaymentItem(element));
        }
        return compatiblePaymentItems;
    }

    @Contract("!null -> !null")
    public static WebPaymentIntentHelperType.@Nullable PaymentShippingOption
            fromMojoPaymentShippingOption(@Nullable PaymentShippingOption shippingOption) {
        if (shippingOption == null) return null;
        return new WebPaymentIntentHelperType.PaymentShippingOption(
                shippingOption.id,
                shippingOption.label,
                fromMojoPaymentCurrencyAmount(shippingOption.amount),
                shippingOption.selected);
    }

    @Contract("!null -> !null")
    public static @Nullable
            List<WebPaymentIntentHelperType.PaymentShippingOption> fromMojoShippingOptionList(
                    @Nullable List<PaymentShippingOption> shippingOptions) {
        if (shippingOptions == null) return null;
        List<WebPaymentIntentHelperType.PaymentShippingOption> shippingOptionList =
                new ArrayList<>();
        for (var element : shippingOptions) {
            shippingOptionList.add(
                    WebPaymentIntentHelperTypeConverter.fromMojoPaymentShippingOption(element));
        }
        return shippingOptionList;
    }

    @Contract("!null -> !null")
    public static WebPaymentIntentHelperType.@Nullable PaymentOptions fromMojoPaymentOptions(
            @Nullable PaymentOptions paymentOptions) {
        if (paymentOptions == null) return null;
        String shippingType = null;
        if (paymentOptions.requestShipping) {
            switch (paymentOptions.shippingType) {
                case PaymentShippingType.SHIPPING:
                    shippingType = "shipping";
                    break;
                case PaymentShippingType.DELIVERY:
                    shippingType = "delivery";
                    break;
                case PaymentShippingType.PICKUP:
                    shippingType = "pickup";
                    break;
            }
        }
        return new WebPaymentIntentHelperType.PaymentOptions(
                paymentOptions.requestPayerName,
                paymentOptions.requestPayerEmail,
                paymentOptions.requestPayerPhone,
                paymentOptions.requestShipping,
                shippingType);
    }

    @Contract("!null -> !null")
    @OptimizeAsNonNull
    private static WebPaymentIntentHelperType.PaymentHandlerModifier fromMojoPaymentHandlerModifier(
            PaymentHandlerModifier handlerModifier) {
        return new WebPaymentIntentHelperType.PaymentHandlerModifier(
                fromMojoPaymentCurrencyAmount(handlerModifier.total),
                fromMojoPaymentHandlerMethodData(handlerModifier.methodData));
    }

    @Contract("!null -> !null")
    @OptimizeAsNonNull
    private static List<WebPaymentIntentHelperType.PaymentHandlerModifier>
            fromMojoPaymentHandlerModifierList(List<PaymentHandlerModifier> modifiers) {
        List<WebPaymentIntentHelperType.PaymentHandlerModifier> compatibleModifiers =
                new ArrayList<>();
        PaymentHandlerModifier element = null;
        for (int i = 0; i < modifiers.size(); i++) {
            element = modifiers.get(i);
            if (element != null) {
                compatibleModifiers.add(fromMojoPaymentHandlerModifier(element));
            }
        }
        return compatibleModifiers;
    }

    private static WebPaymentIntentHelperType.@Nullable PaymentHandlerMethodData
            fromMojoPaymentHandlerMethodData(@Nullable PaymentHandlerMethodData methodData) {
        if (methodData == null) return null;
        return new WebPaymentIntentHelperType.PaymentHandlerMethodData(
                /* methodName= */ methodData.methodName,
                /* stringifiedData= */ methodData.stringifiedData);
    }

    @Contract("!null -> !null")
    private static @Nullable Bundle fromMojoShippingAddressErrors(
            @Nullable AddressErrors addressErrors) {
        if (addressErrors == null) return null;
        Bundle bundle = new Bundle();
        putIfNonEmpty("addressLine", addressErrors.addressLine, bundle);
        putIfNonEmpty("city", addressErrors.city, bundle);
        putIfNonEmpty("countryCode", addressErrors.country, bundle);
        putIfNonEmpty("dependentLocality", addressErrors.dependentLocality, bundle);
        putIfNonEmpty("organization", addressErrors.organization, bundle);
        putIfNonEmpty("phone", addressErrors.phone, bundle);
        putIfNonEmpty("postalCode", addressErrors.postalCode, bundle);
        putIfNonEmpty("recipient", addressErrors.recipient, bundle);
        putIfNonEmpty("region", addressErrors.region, bundle);
        putIfNonEmpty("sortingCode", addressErrors.sortingCode, bundle);
        return bundle;
    }

    private static void putIfNonEmpty(String key, String value, Bundle bundle) {
        if (!TextUtils.isEmpty(value)) bundle.putString(key, value);
    }

    /**
     * Converts PaymentRequestDetailsUpdate from mojo to
     * WebPaymentIntentHelperType.PaymentRequestDetailsUpdate.
     * @param update The mojo PaymentRequestDetailsUpdate to be converted.
     * @return The converted update.
     */
    @Contract("!null -> !null")
    public static WebPaymentIntentHelperType.@Nullable PaymentRequestDetailsUpdate
            fromMojoPaymentRequestDetailsUpdate(@Nullable PaymentRequestDetailsUpdate update) {
        if (update == null) return null;
        return new WebPaymentIntentHelperType.PaymentRequestDetailsUpdate(
                fromMojoPaymentCurrencyAmount(update.total),
                // Arrays.asList does not accept null.
                update.shippingOptions == null
                        ? null
                        : fromMojoShippingOptionList(Arrays.asList(update.shippingOptions)),
                update.modifiers == null
                        ? null
                        : fromMojoPaymentHandlerModifierList(Arrays.asList(update.modifiers)),
                update.error,
                update.stringifiedPaymentMethodErrors,
                fromMojoShippingAddressErrors(update.shippingAddressErrors));
    }
}
