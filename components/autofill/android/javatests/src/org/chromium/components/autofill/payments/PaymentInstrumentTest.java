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
}
