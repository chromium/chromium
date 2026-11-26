// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.secure_payment_confirmation;

import static java.util.Collections.emptyList;

import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.payments.PaymentApp.PaymentEntityLogo;
import org.chromium.components.payments.SPCTransactionMode;
import org.chromium.payments.mojom.PaymentCurrencyAmount;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.util.Arrays;
import java.util.List;

/** Shared test fixtures for Secure Payment Confirmation instrumentation tests. */
@NullMarked
final class SecurePaymentConfirmationTestUtils {
    private SecurePaymentConfirmationTestUtils() {}

    static SecurePaymentConfirmationController createController(
            WindowAndroid window, Callback<Integer> responseCallback) {
        return createController(
                window,
                emptyList(),
                /* primaryLabelsOnly= */ false,
                /* showOptOut= */ false,
                /* informOnly= */ false,
                responseCallback);
    }

    static SecurePaymentConfirmationController createController(
            WindowAndroid window,
            List<PaymentEntityLogo> paymentEntityLogos,
            boolean primaryLabelsOnly,
            boolean showOptOut,
            boolean informOnly,
            Callback<Integer> responseCallback) {
        PaymentItem paymentItem = new PaymentItem();
        paymentItem.amount = new PaymentCurrencyAmount();
        paymentItem.amount.currency = "CAD";
        paymentItem.amount.value = "1.50";

        final int paymentItemIconWidth = 40;
        final int paymentItemIconHeight = 20;
        int[] redBitmapArray = new int[paymentItemIconWidth * paymentItemIconHeight];
        Arrays.fill(redBitmapArray, Color.RED);

        return new SecurePaymentConfirmationController(
                window,
                paymentEntityLogos,
                /* payeeName= */ "The Store",
                /* payeeOrigin= */ primaryLabelsOnly
                        ? null
                        : Origin.create(new GURL("https://test.store")),
                /* paymentInstrumentLabelPrimary= */ "My Credit Card",
                /* paymentInstrumentLabelSecondary= */ primaryLabelsOnly ? null : "•••• 1234",
                paymentItem,
                new BitmapDrawable(
                        ContextUtils.getApplicationContext().getResources(),
                        Bitmap.createBitmap(
                                redBitmapArray,
                                /* width= */ paymentItemIconWidth,
                                /* height= */ paymentItemIconHeight,
                                Config.ARGB_8888)),
                /* relyingPartyId= */ "test.store",
                showOptOut,
                informOnly,
                responseCallback,
                SPCTransactionMode.NONE);
    }
}
