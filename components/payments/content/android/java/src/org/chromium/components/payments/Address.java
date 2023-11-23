// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import android.os.Bundle;

import androidx.annotation.Nullable;

import java.util.regex.Pattern;

/**
 * An immutable class that mirrors org.chromium.payments.mojom.PaymentAddress.
 * https://w3c.github.io/payment-request/#paymentaddress-interface
 */
public class Address {
    /**
     * The pattern for a valid country code:
     * https://w3c.github.io/payment-request/#internal-constructor
     */
    private static final String COUNTRY_CODE_PATTERN = "^[A-Z]{2}$";

    @Nullable private static Pattern sCountryCodePattern;

    public final String country;
    public final String[] addressLine;
    public final String region;
    public final String city;
    public final String dependentLocality;
    public final String postalCode;
    public final String sortingCode;
    public final String organization;
    public final String recipient;
    public final String phone;

    public Address() {
        country = "";
        addressLine = new String[0];
        region = "";
        city = "";
        dependentLocality = "";
        postalCode = "";
        sortingCode = "";
        organization = "";
        recipient = "";
        phone = "";
    }

    /**
     * @param country The country corresponding to the address.
     * @param addressLine The most specific part of the address. It can include, for example, a
     *         street name, a house number, apartment number, a rural delivery route, descriptive
     *         instructions, or a post office box number.
     * @param region The top level administrative subdivision of the country. For example, this can
     *         be a state, a province, an oblast, or a prefecture.
     * @param city The city/town portion of the address.
     * @param dependentLocalitly The dependent locality or sublocality within a city. For example,
     *         neighborhoods, boroughs, districts, or UK dependent localities.
     * @param postalCode The postal code or ZIP code, also known as PIN code in India.
     * @param sortingCode The sorting code as used in, for example, France.
     * @param organization The organization, firm, company, or institution at the address.
     * @param recipient The name of the recipient or contact person at the address.
     * @param phone The phone number of the recipient or contact person at the address.
     */
    public Address(
            String country,
            String[] addressLine,
            String region,
            String city,
            String dependentLocality,
            String postalCode,
            String sortingCode,
            String organization,
            String recipient,
            String phone) {
        this.country = country;
        this.addressLine = addressLine;
        this.region = region;
        this.city = city;
        this.dependentLocality = dependentLocality;
        this.postalCode = postalCode;
        this.sortingCode = sortingCode;
        this.organization = organization;
        this.recipient = recipient;
        this.phone = phone;
    }

    // Keys in shipping address bundle.
    public static final String EXTRA_ADDRESS_COUNTRY = "countryCode";
    public static final String EXTRA_ADDRESS_LINES = "addressLines";
    public static final String EXTRA_ADDRESS_REGION = "region";
    public static final String EXTRA_ADDRESS_CITY = "city";
    public static final String EXTRA_ADDRESS_DEPENDENT_LOCALITY = "dependentLocality";
    public static final String EXTRA_ADDRESS_POSTAL_CODE = "postalCode";
    public static final String EXTRA_ADDRESS_SORTING_CODE = "sortingCode";
    public static final String EXTRA_ADDRESS_ORGANIZATION = "organization";
    public static final String EXTRA_ADDRESS_RECIPIENT = "recipient";
    public static final String EXTRA_ADDRESS_PHONE = "phone";

    /**
     * @param address Bundle to be parsed.
     * @return converted Address or null.
     */
    @Nullable
    public static Address createFromBundle(@Nullable Bundle address) {
        if (address == null) return null;
        return new Address(
                getStringOrEmpty(address, EXTRA_ADDRESS_COUNTRY),
                address.getStringArray(EXTRA_ADDRESS_LINES),
                getStringOrEmpty(address, EXTRA_ADDRESS_REGION),
                getStringOrEmpty(address, EXTRA_ADDRESS_CITY),
                getStringOrEmpty(address, EXTRA_ADDRESS_DEPENDENT_LOCALITY),
                getStringOrEmpty(address, EXTRA_ADDRESS_POSTAL_CODE),
                getStringOrEmpty(address, EXTRA_ADDRESS_SORTING_CODE),
                getStringOrEmpty(address, EXTRA_ADDRESS_ORGANIZATION),
                getStringOrEmpty(address, EXTRA_ADDRESS_RECIPIENT),
                getStringOrEmpty(address, EXTRA_ADDRESS_PHONE));
    }

    private static String getStringOrEmpty(Bundle bundle, String key) {
        return bundle.getString(key, /* defaultValue= */ "");
    }

    public boolean isValid() {
        if (sCountryCodePattern == null) {
            sCountryCodePattern = Pattern.compile(COUNTRY_CODE_PATTERN);
        }
        return sCountryCodePattern.matcher(country).matches();
    }
}
