// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.payments;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertThrows;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.url.GURL;

/** Unit tests for {@link PaymentInstrument} */
@RunWith(BaseRobolectricTestRunner.class)
public class PaymentInstrumentTest {

    @Test
    public void paymentInstrument_build() {
        PaymentInstrument paymentInstrument =
                new PaymentInstrument.Builder()
                        .setInstrumentId(100)
                        .setNickname("nickname")
                        .setDisplayIconUrl(new GURL("http://www.example.com"))
                        .setSupportedPaymentRails(new int[] {1})
                        .build();

        assertThat(paymentInstrument.getInstrumentId()).isEqualTo(100);
        assertThat(paymentInstrument.getNickname()).isEqualTo("nickname");
        assertThat(paymentInstrument.getDisplayIconUrl())
                .isEqualTo(new GURL("http://www.example.com"));
        assertThat(paymentInstrument.isSupported(1)).isTrue();
    }

    @Test
    public void paymentInstrument_instrumentIdNotSet() {
        AssertionError error =
                assertThrows(
                        AssertionError.class,
                        () ->
                                new PaymentInstrument.Builder()
                                        .setNickname("nickname")
                                        .setDisplayIconUrl(new GURL("http://www.example.com"))
                                        .setSupportedPaymentRails(new int[] {1})
                                        .build());

        assertThat(error.getMessage())
                .isEqualTo("InstrumentId must be set for the payment instrument.");
    }

    @Test
    public void paymentInstrument_noSupportedRails() {
        AssertionError error =
                assertThrows(
                        AssertionError.class,
                        () ->
                                new PaymentInstrument.Builder()
                                        .setInstrumentId(100)
                                        .setNickname("nickname")
                                        .setDisplayIconUrl(new GURL("http://www.example.com"))
                                        .build());

        assertThat(error.getMessage())
                .isEqualTo("Payment instrument must support at least one payment rail.");
    }

    @Test
    public void paymentInstrument_emptySupportedRails() {
        AssertionError error =
                assertThrows(
                        AssertionError.class,
                        () ->
                                new PaymentInstrument.Builder()
                                        .setInstrumentId(100)
                                        .setNickname("nickname")
                                        .setDisplayIconUrl(new GURL("http://www.example.com"))
                                        .setSupportedPaymentRails(new int[] {})
                                        .build());

        assertThat(error.getMessage())
                .isEqualTo("Payment instrument must support at least one payment rail.");
    }

    @Test
    public void isSupported() {
        PaymentInstrument paymentInstrument =
                new PaymentInstrument.Builder()
                        .setInstrumentId(100)
                        .setNickname("nickname")
                        .setDisplayIconUrl(new GURL("http://www.example.com"))
                        .setSupportedPaymentRails(new int[] {1})
                        .build();

        assertThat(paymentInstrument.isSupported(1)).isTrue();
        assertThat(paymentInstrument.isSupported(2)).isFalse();
    }

    @Test
    public void testEquals_differentInstrumentId_returnsFalse() {
        PaymentInstrument paymentInstrument1 =
                new PaymentInstrument.Builder()
                        .setInstrumentId(100)
                        .setNickname("nickname")
                        .setDisplayIconUrl(new GURL("http://www.example.com"))
                        .setSupportedPaymentRails(new int[] {1})
                        .build();
        PaymentInstrument paymentInstrument2 =
                new PaymentInstrument.Builder()
                        .setInstrumentId(200)
                        .setNickname("nickname")
                        .setDisplayIconUrl(new GURL("http://www.example.com"))
                        .setSupportedPaymentRails(new int[] {1})
                        .build();

        assertThat(paymentInstrument1.equals(paymentInstrument2)).isFalse();
    }

    @Test
    public void testEquals_differentNickname_returnsFalse() {
        PaymentInstrument paymentInstrument1 =
                new PaymentInstrument.Builder()
                        .setInstrumentId(100)
                        .setNickname("nickname1")
                        .setDisplayIconUrl(new GURL("http://www.example.com"))
                        .setSupportedPaymentRails(new int[] {1})
                        .build();
        PaymentInstrument paymentInstrument2 =
                new PaymentInstrument.Builder()
                        .setInstrumentId(100)
                        .setNickname("nickname2")
                        .setDisplayIconUrl(new GURL("http://www.example.com"))
                        .setSupportedPaymentRails(new int[] {1})
                        .build();

        assertThat(paymentInstrument1.equals(paymentInstrument2)).isFalse();
    }

    @Test
    public void testEquals_differentDisplayIconUrl_returnsFalse() {
        PaymentInstrument paymentInstrument1 =
                new PaymentInstrument.Builder()
                        .setInstrumentId(100)
                        .setNickname("nickname")
                        .setDisplayIconUrl(new GURL("http://www.example1.com"))
                        .setSupportedPaymentRails(new int[] {1})
                        .build();
        PaymentInstrument paymentInstrument2 =
                new PaymentInstrument.Builder()
                        .setInstrumentId(100)
                        .setNickname("nickname")
                        .setDisplayIconUrl(new GURL("http://www.example2.com"))
                        .setSupportedPaymentRails(new int[] {1})
                        .build();

        assertThat(paymentInstrument1.equals(paymentInstrument2)).isFalse();
    }

    @Test
    public void testEquals_differentPaymentSupportedRails_returnsFalse() {
        PaymentInstrument paymentInstrument1 =
                new PaymentInstrument.Builder()
                        .setInstrumentId(100)
                        .setNickname("nickname")
                        .setDisplayIconUrl(new GURL("http://www.example.com"))
                        .setSupportedPaymentRails(new int[] {2})
                        .build();
        PaymentInstrument paymentInstrument2 =
                new PaymentInstrument.Builder()
                        .setInstrumentId(100)
                        .setNickname("nickname")
                        .setDisplayIconUrl(new GURL("http://www.example.com"))
                        .setSupportedPaymentRails(new int[] {1})
                        .build();

        assertThat(paymentInstrument1.equals(paymentInstrument2)).isFalse();
    }

    @Test
    public void testEquals_samePaymentInstruments_returnsTrue() {
        PaymentInstrument paymentInstrument1 =
                new PaymentInstrument.Builder()
                        .setInstrumentId(100)
                        .setNickname("nickname")
                        .setDisplayIconUrl(new GURL("http://www.example.com"))
                        .setSupportedPaymentRails(new int[] {1})
                        .build();
        PaymentInstrument paymentInstrument2 =
                new PaymentInstrument.Builder()
                        .setInstrumentId(100)
                        .setNickname("nickname")
                        .setDisplayIconUrl(new GURL("http://www.example.com"))
                        .setSupportedPaymentRails(new int[] {1})
                        .build();

        assertThat(paymentInstrument1.equals(paymentInstrument2)).isTrue();
    }

    @Test
    public void testEquals_nullObject_returnsFalse() {
        PaymentInstrument paymentInstrument =
                new PaymentInstrument.Builder()
                        .setInstrumentId(100)
                        .setNickname("nickname")
                        .setDisplayIconUrl(new GURL("http://www.example.com"))
                        .setSupportedPaymentRails(new int[] {1})
                        .build();

        assertThat(paymentInstrument.equals(null)).isFalse();
    }

    @Test
    public void testEquals_noPaymentInstrumentObject_returnsFalse() {
        PaymentInstrument paymentInstrument =
                new PaymentInstrument.Builder()
                        .setInstrumentId(100)
                        .setNickname("nickname")
                        .setDisplayIconUrl(new GURL("http://www.example.com"))
                        .setSupportedPaymentRails(new int[] {1})
                        .build();

        // Create an object which is not a BankAccount.
        Integer noPaymentInstrumentObject = Integer.valueOf(1);
        assertThat(paymentInstrument.equals((Object) noPaymentInstrumentObject)).isFalse();
    }
}
