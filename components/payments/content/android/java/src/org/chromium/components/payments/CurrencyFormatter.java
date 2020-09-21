// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

import java.util.Locale;

/**
 * Formatter for currency amounts.
 * https://w3c.github.io/browser-payment-api/specs/paymentrequest.html#currencyamount
 */
@JNINamespace("payments")
public class CurrencyFormatter {
    /**
     * Pointer to the native implementation.
     */
    private long mCurrencyFormatterAndroid;

    /**
     * Builds the formatter for the given currency code and the current user locale.
     *
     * @param currencyCode  The currency code. Most commonly, this follows ISO 4217 format: 3 upper
     *                      case ASCII letters. For example, "USD". Format is not restricted. Should
     *                      not be null.
     * @param userLocale User's current locale. Should not be null.
     */
    public CurrencyFormatter(String currencyCode, Locale userLocale) {
        assert currencyCode != null : "currencyCode should not be null";
        assert userLocale != null : "userLocale should not be null";

        // Note that this pointer could leak the native object. The called must call destroy() to
        // ensure that the native object is destroyed.
        mCurrencyFormatterAndroid = CurrencyFormatterJni.get().initCurrencyFormatterAndroid(
                CurrencyFormatter.this, currencyCode, userLocale.toString());
    }

    /** Will destroy the native object. This class shouldn't be used afterwards. */
    public void destroy() {
        if (mCurrencyFormatterAndroid != 0) {
            CurrencyFormatterJni.get().destroy(mCurrencyFormatterAndroid, CurrencyFormatter.this);
            mCurrencyFormatterAndroid = 0;
        }
    }

    /** @return The currency code formatted for display. */
    public String getFormattedCurrencyCode() {
        return CurrencyFormatterJni.get().getFormattedCurrencyCode(
                mCurrencyFormatterAndroid, CurrencyFormatter.this);
    }

    /**
     * Formats the currency string for display. Does not parse the string into a number, because it
     * might be too large. The number is formatted for the current locale and can include a
     * currency symbol (e.g. $) anywhere in the string, but will not contain the currency code
     * (e.g. USD/US). All spaces in the currency are unicode non-breaking space.
     *
     * @param amountValue The number to format. Should be in "^-?[0-9]+(\.[0-9]+)?$" format. Should
     *                    not be null.
     * @return The amount formatted with the specified currency. See description for details.
     */
    public String format(String amountValue) {
        assert amountValue != null : "amountValue should not be null";

        return CurrencyFormatterJni.get().format(
                mCurrencyFormatterAndroid, CurrencyFormatter.this, amountValue);
    }

    @NativeMethods
    interface Natives {
        long initCurrencyFormatterAndroid(
                CurrencyFormatter caller, String currencyCode, String localeName);
        void destroy(long nativeCurrencyFormatterAndroid, CurrencyFormatter caller);
        String format(
                long nativeCurrencyFormatterAndroid, CurrencyFormatter caller, String amountValue);
        String getFormattedCurrencyCode(
                long nativeCurrencyFormatterAndroid, CurrencyFormatter caller);
    }
}
