// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import androidx.annotation.Nullable;

import org.chromium.payments.mojom.PaymentOptions;

/** A collection of utility methods for PaymentOptions. */
public class PaymentOptionsUtils {
    /**
     * @param options Any PaymentOption, can be null.
     * @return Whether a PaymentOptions has requested any information (shipping, payer's email,
     *         payer's phone, payer's name).
     */
    public static boolean requestAnyInformation(@Nullable PaymentOptions options) {
        if (options == null) return false;
        return options.requestShipping
                || options.requestPayerEmail
                || options.requestPayerPhone
                || options.requestPayerName;
    }

    /**
     * @param options Any PaymentOption, can be null.
     * @return Whether a PaymentOptions has requested any payer's information (email, phone, name).
     */
    public static boolean requestAnyContactInformation(@Nullable PaymentOptions options) {
        if (options == null) return false;
        return options.requestPayerEmail || options.requestPayerPhone || options.requestPayerName;
    }

    /**
     * @param options Any PaymentOptions, can be null.
     * @return Return a JSON string indicating whether each information is requested in the
     *         PaymentOptions.
     */
    public static String stringifyRequestedInformation(@Nullable PaymentOptions options) {
        boolean requestPayerEmail = false;
        boolean requestPayerName = false;
        boolean requestPayerPhone = false;
        boolean requestShipping = false;
        if (options != null) {
            requestPayerEmail = options.requestPayerEmail;
            requestPayerName = options.requestPayerName;
            requestPayerPhone = options.requestPayerPhone;
            requestShipping = options.requestShipping;
        }
        return String.format(
                "{payerEmail:%s,payerName:%s,payerPhone:%s,shipping:%s}",
                requestPayerEmail, requestPayerName, requestPayerPhone, requestShipping);
    }
}
