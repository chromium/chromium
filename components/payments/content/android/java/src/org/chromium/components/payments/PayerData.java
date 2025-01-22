// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** An immutable class used to bundle the payer data received from payment handlers. */
@NullMarked
public class PayerData {
    public final @Nullable String payerName;
    public final @Nullable String payerPhone;
    public final @Nullable String payerEmail;
    public final @Nullable Address shippingAddress;
    public final @Nullable String selectedShippingOptionId;

    /**
     * @param payerName The payer's name.
     * @param payerPhone The payer's phone number.
     * @param payerEmail The payer's email address.
     * @param shippingAddress The user selected shippingAddress.
     * @param selectedShippingOptionId The user selected shipping option's identifier.
     */
    public PayerData(
            String payerName,
            String payerPhone,
            String payerEmail,
            @Nullable Address shippingAddress,
            String selectedShippingOptionId) {
        this.payerName = payerName;
        this.payerPhone = payerPhone;
        this.payerEmail = payerEmail;
        this.shippingAddress = shippingAddress;
        this.selectedShippingOptionId = selectedShippingOptionId;
    }

    public PayerData() {
        payerName = null;
        payerPhone = null;
        payerEmail = null;
        shippingAddress = null;
        selectedShippingOptionId = null;
    }
}
