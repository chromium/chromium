// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import androidx.annotation.Nullable;

import org.chromium.base.Log;

/** This class represents the supported delegations of a service worker based payment app. */
public class SupportedDelegations {
    private static final String TAG = "SupportedDelegations";
    private final boolean mShippingAddress;
    private final boolean mPayerName;
    private final boolean mPayerPhone;
    private final boolean mPayerEmail;

    public SupportedDelegations(
            boolean shippingAddress, boolean payerName, boolean payerPhone, boolean payerEmail) {
        mShippingAddress = shippingAddress;
        mPayerName = payerName;
        mPayerPhone = payerPhone;
        mPayerEmail = payerEmail;
    }

    public SupportedDelegations() {
        mShippingAddress = false;
        mPayerName = false;
        mPayerPhone = false;
        mPayerEmail = false;
    }

    public boolean providesAll(org.chromium.payments.mojom.PaymentOptions options) {
        if (options == null) return true;
        if (options.requestShipping && !mShippingAddress) return false;
        if (options.requestPayerName && !mPayerName) return false;
        if (options.requestPayerPhone && !mPayerPhone) return false;
        if (options.requestPayerEmail && !mPayerEmail) return false;
        return true;
    }

    public boolean getShippingAddress() {
        return mShippingAddress;
    }

    public boolean getPayerName() {
        return mPayerName;
    }

    public boolean getPayerPhone() {
        return mPayerPhone;
    }

    public boolean getPayerEmail() {
        return mPayerEmail;
    }

    public static SupportedDelegations createFromStringArray(
            @Nullable String[] supportedDelegationsNames) throws IllegalArgumentException {
        if (supportedDelegationsNames == null || supportedDelegationsNames.length == 0) {
            return new SupportedDelegations();
        }

        boolean shippingAddress = false;
        boolean payerName = false;
        boolean payerPhone = false;
        boolean payerEmail = false;

        // At most check the first 4 elements since there are only 4 different valid delegation
        // types.
        final int cappedArraySize =
                Math.min(supportedDelegationsNames.length, /* MAX_DELEGATION_SIZE= */ 4);
        for (int i = 0; i < cappedArraySize; i++) {
            if (supportedDelegationsNames[i] == null) {
                Log.e(
                        TAG,
                        "null is an invalid delegation value. Only [\"shippingAddress\", "
                                + "\"payerName\", \"payerPhone\", \"payerEmail\"] values "
                                + "are possible.");
            } else if (supportedDelegationsNames[i].equals("shippingAddress")) {
                shippingAddress = true;
            } else if (supportedDelegationsNames[i].equals("payerName")) {
                payerName = true;
            } else if (supportedDelegationsNames[i].equals("payerPhone")) {
                payerPhone = true;
            } else if (supportedDelegationsNames[i].equals("payerEmail")) {
                payerEmail = true;
            } else {
                Log.e(
                        TAG,
                        "\"%s\" is an invalid delegation value. Only [\"shippingAddress\", "
                                + "\"payerName\", \"payerPhone\", \"payerEmail\"] values are "
                                + "possible.",
                        supportedDelegationsNames[i]);
            }
        }
        return new SupportedDelegations(shippingAddress, payerName, payerPhone, payerEmail);
    }
}
