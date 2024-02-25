// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import androidx.annotation.Nullable;

import org.chromium.payments.mojom.PaymentAddress;

/**
 * A Utility class that converts org.chromium.components.payments.Address type to
 * org.chromium.payments.mojom.PaymentAddress and vice versa.
 */
public final class PaymentAddressTypeConverter {
    /**
     * @param address The org.chromium.payments.mojom.PaymentAddress to be converted.
     * @return The converted address with type org.chromium.components.payments.Address.
     */
    @Nullable
    public static Address convertPaymentAddressFromMojo(PaymentAddress address) {
        if (address == null) return null;
        return new Address(
                address.country,
                address.addressLine,
                address.region,
                address.city,
                address.dependentLocality,
                address.postalCode,
                address.sortingCode,
                address.organization,
                address.recipient,
                address.phone);
    }

    /**
     * @param address The org.chromium.components.payments.Address address to be converted.
     * @return The converted address with type org.chromium.payments.mojom.PaymentAddress.
     */
    @Nullable
    public static PaymentAddress convertAddressToMojoPaymentAddress(Address address) {
        if (address == null) return null;
        PaymentAddress result = new PaymentAddress();
        result.country = address.country;
        result.addressLine = address.addressLine;
        result.region = address.region;
        result.city = address.city;
        result.dependentLocality = address.dependentLocality;
        result.postalCode = address.postalCode;
        result.sortingCode = address.sortingCode;
        result.organization = address.organization;
        result.recipient = address.recipient;
        result.phone = address.phone;
        return result;
    }
}
