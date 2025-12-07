// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.intent;

import static org.chromium.components.payments.intent.WebPaymentIntentHelperType.PaymentCurrencyAmount.EXTRA_CURRENCY;
import static org.chromium.components.payments.intent.WebPaymentIntentHelperType.PaymentCurrencyAmount.EXTRA_VALUE;
import static org.chromium.components.payments.intent.WebPaymentIntentHelperType.PaymentHandlerMethodData.EXTRA_METHOD_NAME;
import static org.chromium.components.payments.intent.WebPaymentIntentHelperType.PaymentHandlerMethodData.EXTRA_STRINGIFIED_DETAILS;
import static org.chromium.components.payments.intent.WebPaymentIntentHelperType.PaymentHandlerModifier.EXTRA_METHOD_DATA;
import static org.chromium.components.payments.intent.WebPaymentIntentHelperType.PaymentRequestDetailsUpdate.EXTRA_MODIFIERS;
import static org.chromium.components.payments.intent.WebPaymentIntentHelperType.PaymentRequestDetailsUpdate.EXTRA_TOTAL;

import android.os.Bundle;
import android.os.Parcelable;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.payments.mojom.PaymentCurrencyAmount;
import org.chromium.payments.mojom.PaymentHandlerMethodData;
import org.chromium.payments.mojom.PaymentHandlerModifier;
import org.chromium.payments.mojom.PaymentRequestDetailsUpdate;

/**
 * Tests for converting mojo data from the merchant website into Android intents data for the
 * invoked Android payment app.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class WebPaymentIntentHelperTypeConverterTest {
    /**
     * A null mojo update from the merchant website results in a null Android intent update for the
     * Android payment app that was invoked by the merchant website.
     */
    @Test
    @Feature({"Payments"})
    public void fromNullMojoPaymentRequestDetailsUpdate() throws Exception {
        Assert.assertNull(
                WebPaymentIntentHelperTypeConverter.fromMojoPaymentRequestDetailsUpdate(
                        /* update= */ null));
    }

    /**
     * If the merchant website updates the overall total amount, that data is passed to the Android
     * payment app that was invoked by the merchant website.
     */
    @Test
    @Feature({"Payments"})
    public void fromMojoPaymentRequestDetailsUpdateWithTotal() throws Exception {
        PaymentRequestDetailsUpdate mojoUpdate = new PaymentRequestDetailsUpdate();
        mojoUpdate.total = new PaymentCurrencyAmount();
        mojoUpdate.total.currency = "CAD";
        mojoUpdate.total.value = "0.01";

        WebPaymentIntentHelperType.PaymentRequestDetailsUpdate intentUpdate =
                WebPaymentIntentHelperTypeConverter.fromMojoPaymentRequestDetailsUpdate(mojoUpdate);
        Assert.assertNotNull(intentUpdate);

        Bundle updateBundle = intentUpdate.asBundle();
        Assert.assertNotNull(updateBundle);
        Assert.assertNull(updateBundle.getParcelableArray(EXTRA_MODIFIERS));

        Bundle totalBundle = updateBundle.getBundle(EXTRA_TOTAL);
        Assert.assertNotNull(totalBundle);
        Assert.assertEquals("CAD", totalBundle.getString(EXTRA_CURRENCY));
        Assert.assertEquals("0.01", totalBundle.getString(EXTRA_VALUE));
    }

    /**
     * If the merchant website sends a null modifier in the payment details update, the invoked
     * Android payment app does not receive any modifiers.
     */
    @Test
    @Feature({"Payments"})
    public void fromMojoPaymentRequestDetailsUpdateWithOneNullModifier() throws Exception {
        PaymentRequestDetailsUpdate mojoUpdate = new PaymentRequestDetailsUpdate();
        mojoUpdate.modifiers = new PaymentHandlerModifier[] {null};

        WebPaymentIntentHelperType.PaymentRequestDetailsUpdate intentUpdate =
                WebPaymentIntentHelperTypeConverter.fromMojoPaymentRequestDetailsUpdate(mojoUpdate);
        Assert.assertNotNull(intentUpdate);

        Bundle updateBundle = intentUpdate.asBundle();
        Assert.assertNotNull(updateBundle);

        Assert.assertNull(updateBundle.getParcelableArray(EXTRA_MODIFIERS));
    }

    /**
     * If the merchant website sends an empty modifier in the payment details update, the invoked
     * Android payment app receives an empty bundle.
     */
    @Test
    @Feature({"Payments"})
    public void fromMojoPaymentRequestDetailsUpdateWithEmptyModifier() throws Exception {
        PaymentRequestDetailsUpdate mojoUpdate = new PaymentRequestDetailsUpdate();
        mojoUpdate.modifiers = new PaymentHandlerModifier[] {new PaymentHandlerModifier()};

        WebPaymentIntentHelperType.PaymentRequestDetailsUpdate intentUpdate =
                WebPaymentIntentHelperTypeConverter.fromMojoPaymentRequestDetailsUpdate(mojoUpdate);
        Assert.assertNotNull(intentUpdate);

        Bundle updateBundle = intentUpdate.asBundle();
        Assert.assertNotNull(updateBundle);

        Parcelable[] parcelableModifiers = updateBundle.getParcelableArray(EXTRA_MODIFIERS);
        Assert.assertNotNull(parcelableModifiers);
        Assert.assertEquals(1, parcelableModifiers.length);
        Assert.assertTrue(parcelableModifiers[0] instanceof Bundle);

        Bundle modifierBundle = (Bundle) parcelableModifiers[0];
        Assert.assertEquals(0, modifierBundle.size());
    }

    /**
     * If the merchant website sends a modifier update that includes both total amount and method
     * data, that modifier is sent to the Android payment app that was invoked by the merchant
     * website.
     */
    @Test
    @Feature({"Payments"})
    public void fromMojoPaymentRequestDetailsUpdateWithModifierAndTotal() throws Exception {
        PaymentRequestDetailsUpdate mojoUpdate = new PaymentRequestDetailsUpdate();
        mojoUpdate.modifiers = new PaymentHandlerModifier[] {new PaymentHandlerModifier()};
        mojoUpdate.modifiers[0].total = new PaymentCurrencyAmount();
        mojoUpdate.modifiers[0].total.currency = "MEX";
        mojoUpdate.modifiers[0].total.value = "0.02";
        mojoUpdate.modifiers[0].methodData = new PaymentHandlerMethodData();
        mojoUpdate.modifiers[0].methodData.methodName = "https://payments.example";
        mojoUpdate.modifiers[0].methodData.stringifiedData = "{\"key\":\"value\"}";

        WebPaymentIntentHelperType.PaymentRequestDetailsUpdate intentUpdate =
                WebPaymentIntentHelperTypeConverter.fromMojoPaymentRequestDetailsUpdate(mojoUpdate);
        Assert.assertNotNull(intentUpdate);

        Bundle updateBundle = intentUpdate.asBundle();
        Assert.assertNotNull(updateBundle);

        Parcelable[] parcelableModifiers = updateBundle.getParcelableArray(EXTRA_MODIFIERS);
        Assert.assertNotNull(parcelableModifiers);
        Assert.assertEquals(1, parcelableModifiers.length);
        Assert.assertTrue(parcelableModifiers[0] instanceof Bundle);

        Bundle modifierBundle = (Bundle) parcelableModifiers[0];
        Bundle totalBundle = modifierBundle.getBundle(EXTRA_TOTAL);
        Assert.assertNotNull(totalBundle);
        Assert.assertEquals("MEX", totalBundle.getString(EXTRA_CURRENCY));
        Assert.assertEquals("0.02", totalBundle.getString(EXTRA_VALUE));

        Bundle methodDataBundle = modifierBundle.getBundle(EXTRA_METHOD_DATA);
        Assert.assertNotNull(methodDataBundle);
        Assert.assertEquals(
                "https://payments.example", methodDataBundle.getString(EXTRA_METHOD_NAME));
        Assert.assertEquals(
                "{\"key\":\"value\"}", methodDataBundle.getString(EXTRA_STRINGIFIED_DETAILS));
    }

    /**
     * If the merchant website sends a methodData-only modifier in a payment details update, that
     * data is sent to the Android payment app that was invoked by the merchant website.
     */
    @Test
    @Feature({"Payments"})
    public void fromMojoPaymentRequestDetailsUpdateWithModifier() throws Exception {
        PaymentRequestDetailsUpdate mojoUpdate = new PaymentRequestDetailsUpdate();
        mojoUpdate.modifiers = new PaymentHandlerModifier[] {new PaymentHandlerModifier()};
        mojoUpdate.modifiers[0].methodData = new PaymentHandlerMethodData();
        mojoUpdate.modifiers[0].methodData.methodName = "https://payments.example";
        mojoUpdate.modifiers[0].methodData.stringifiedData = "{\"key\":\"value\"}";

        WebPaymentIntentHelperType.PaymentRequestDetailsUpdate intentUpdate =
                WebPaymentIntentHelperTypeConverter.fromMojoPaymentRequestDetailsUpdate(mojoUpdate);
        Assert.assertNotNull(intentUpdate);

        Bundle updateBundle = intentUpdate.asBundle();
        Assert.assertNotNull(updateBundle);

        Parcelable[] parcelableModifiers = updateBundle.getParcelableArray(EXTRA_MODIFIERS);
        Assert.assertNotNull(parcelableModifiers);
        Assert.assertEquals(1, parcelableModifiers.length);
        Assert.assertTrue(parcelableModifiers[0] instanceof Bundle);

        Bundle modifierBundle = (Bundle) parcelableModifiers[0];
        Assert.assertNull(modifierBundle.getBundle(EXTRA_TOTAL));

        Bundle methodDataBundle = modifierBundle.getBundle(EXTRA_METHOD_DATA);
        Assert.assertNotNull(methodDataBundle);
        Assert.assertEquals(
                "https://payments.example", methodDataBundle.getString(EXTRA_METHOD_NAME));
        Assert.assertEquals(
                "{\"key\":\"value\"}", methodDataBundle.getString(EXTRA_STRINGIFIED_DETAILS));
    }
}
