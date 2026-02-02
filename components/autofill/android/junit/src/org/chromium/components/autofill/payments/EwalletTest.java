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

/** Unit tests for {@link Ewallet} */
@RunWith(BaseRobolectricTestRunner.class)
public class EwalletTest {
    @Test
    public void ewallet_build() {
        Ewallet ewallet =
                new Ewallet.Builder()
                        .setEwalletName("ewallet name")
                        .setAccountDisplayName("account display name")
                        .setPaymentInstrument(
                                new PaymentInstrument.Builder()
                                        .setInstrumentId(100)
                                        .setNickname("nickname")
                                        .setDisplayIconUrl(new GURL("http://www.example.com"))
                                        .setSupportedPaymentRails(new int[] {2})
                                        .setIsFidoEnrolled(true)
                                        .build())
                        .build();

        assertThat(ewallet.getEwalletName()).isEqualTo("ewallet name");
        assertThat(ewallet.getAccountDisplayName()).isEqualTo("account display name");
        assertThat(ewallet.getInstrumentId()).isEqualTo(100);
        assertThat(ewallet.getNickname()).isEqualTo("nickname");
        assertThat(ewallet.getDisplayIconUrl()).isEqualTo(new GURL("http://www.example.com"));
        assertThat(ewallet.getSupportedPaymentRails()).isEqualTo(new int[] {2});
        assertThat(ewallet.getIsFidoEnrolled()).isTrue();
    }

    @Test
    public void ewallet_create() {
        Ewallet ewallet =
                Ewallet.create(
                        /* instrumentId= */ 100,
                        "nickname",
                        new GURL("http://www.example.com"),
                        /* supportedPaymentRails= */ new int[] {2},
                        /* isFidoEnrolled= */ true,
                        "ewallet name",
                        "account display name");

        assertThat(ewallet.getEwalletName()).isEqualTo("ewallet name");
        assertThat(ewallet.getAccountDisplayName()).isEqualTo("account display name");
        assertThat(ewallet.getInstrumentId()).isEqualTo(100);
        assertThat(ewallet.getNickname()).isEqualTo("nickname");
        assertThat(ewallet.getDisplayIconUrl()).isEqualTo(new GURL("http://www.example.com"));
        assertThat(ewallet.getSupportedPaymentRails()).isEqualTo(new int[] {2});
        assertThat(ewallet.getIsFidoEnrolled()).isTrue();
    }

    @Test
    public void testEquals_differentPaymentInstrument_returnsFalse() {
        Ewallet ewallet1 =
                new Ewallet.Builder()
                        .setEwalletName("ewallet name")
                        .setAccountDisplayName("account display name")
                        .setPaymentInstrument(
                                new PaymentInstrument.Builder()
                                        .setInstrumentId(100)
                                        .setNickname("nickname")
                                        .setDisplayIconUrl(new GURL("http://www.example.com"))
                                        .setSupportedPaymentRails(new int[] {2})
                                        .setIsFidoEnrolled(true)
                                        .build())
                        .build();

        Ewallet ewallet2 =
                new Ewallet.Builder()
                        .setEwalletName("ewallet name")
                        .setAccountDisplayName("account display name")
                        .setPaymentInstrument(
                                new PaymentInstrument.Builder()
                                        .setInstrumentId(200)
                                        .setNickname("nickname")
                                        .setDisplayIconUrl(new GURL("http://www.example.com"))
                                        .setSupportedPaymentRails(new int[] {2})
                                        .setIsFidoEnrolled(true)
                                        .build())
                        .build();

        assertThat(ewallet1.equals(ewallet2)).isFalse();
    }

    @Test
    public void testEquals_differentEwalletName_returnsFalse() {
        Ewallet ewallet1 =
                new Ewallet.Builder()
                        .setEwalletName("ewallet name 1")
                        .setAccountDisplayName("account display name")
                        .setPaymentInstrument(
                                new PaymentInstrument.Builder()
                                        .setInstrumentId(100)
                                        .setNickname("nickname")
                                        .setDisplayIconUrl(new GURL("http://www.example.com"))
                                        .setSupportedPaymentRails(new int[] {2})
                                        .setIsFidoEnrolled(true)
                                        .build())
                        .build();

        Ewallet ewallet2 =
                new Ewallet.Builder()
                        .setEwalletName("ewallet name 2")
                        .setAccountDisplayName("account display name")
                        .setPaymentInstrument(
                                new PaymentInstrument.Builder()
                                        .setInstrumentId(100)
                                        .setNickname("nickname")
                                        .setDisplayIconUrl(new GURL("http://www.example.com"))
                                        .setSupportedPaymentRails(new int[] {2})
                                        .setIsFidoEnrolled(true)
                                        .build())
                        .build();

        assertThat(ewallet1.equals(ewallet2)).isFalse();
    }

    @Test
    public void testEquals_differentAccountDisplayName_returnsFalse() {
        Ewallet ewallet1 =
                new Ewallet.Builder()
                        .setEwalletName("ewallet name")
                        .setAccountDisplayName("account display name 1")
                        .setPaymentInstrument(
                                new PaymentInstrument.Builder()
                                        .setInstrumentId(100)
                                        .setNickname("nickname")
                                        .setDisplayIconUrl(new GURL("http://www.example.com"))
                                        .setSupportedPaymentRails(new int[] {2})
                                        .setIsFidoEnrolled(true)
                                        .build())
                        .build();

        Ewallet ewallet2 =
                new Ewallet.Builder()
                        .setEwalletName("ewallet name")
                        .setAccountDisplayName("account display name 2")
                        .setPaymentInstrument(
                                new PaymentInstrument.Builder()
                                        .setInstrumentId(100)
                                        .setNickname("nickname")
                                        .setDisplayIconUrl(new GURL("http://www.example.com"))
                                        .setSupportedPaymentRails(new int[] {2})
                                        .setIsFidoEnrolled(true)
                                        .build())
                        .build();

        assertThat(ewallet1.equals(ewallet2)).isFalse();
    }

    @Test
    public void testEquals_sameEwallets_returnsTrue() {
        Ewallet ewallet1 =
                new Ewallet.Builder()
                        .setEwalletName("ewallet name")
                        .setAccountDisplayName("account display name")
                        .setPaymentInstrument(
                                new PaymentInstrument.Builder()
                                        .setInstrumentId(100)
                                        .setNickname("nickname")
                                        .setDisplayIconUrl(new GURL("http://www.example.com"))
                                        .setSupportedPaymentRails(new int[] {2})
                                        .setIsFidoEnrolled(true)
                                        .build())
                        .build();

        Ewallet ewallet2 =
                new Ewallet.Builder()
                        .setEwalletName("ewallet name")
                        .setAccountDisplayName("account display name")
                        .setPaymentInstrument(
                                new PaymentInstrument.Builder()
                                        .setInstrumentId(100)
                                        .setNickname("nickname")
                                        .setDisplayIconUrl(new GURL("http://www.example.com"))
                                        .setSupportedPaymentRails(new int[] {2})
                                        .setIsFidoEnrolled(true)
                                        .build())
                        .build();

        assertThat(ewallet1.equals(ewallet2)).isTrue();
    }

    @Test
    public void testEquals_nullObject_returnsFalse() {
        Ewallet ewallet1 =
                new Ewallet.Builder()
                        .setEwalletName("ewallet name")
                        .setAccountDisplayName("account display name")
                        .setPaymentInstrument(
                                new PaymentInstrument.Builder()
                                        .setInstrumentId(100)
                                        .setNickname("nickname")
                                        .setDisplayIconUrl(new GURL("http://www.example.com"))
                                        .setSupportedPaymentRails(new int[] {2})
                                        .setIsFidoEnrolled(true)
                                        .build())
                        .build();

        assertThat(ewallet1.equals(null)).isFalse();
    }

    @Test
    public void testEquals_notEwalletObject_returnsFalse() {
        Ewallet ewallet1 =
                new Ewallet.Builder()
                        .setEwalletName("ewallet name")
                        .setAccountDisplayName("account display name")
                        .setPaymentInstrument(
                                new PaymentInstrument.Builder()
                                        .setInstrumentId(100)
                                        .setNickname("nickname")
                                        .setDisplayIconUrl(new GURL("http://www.example.com"))
                                        .setSupportedPaymentRails(new int[] {2})
                                        .setIsFidoEnrolled(true)
                                        .build())
                        .build();

        // Create an object which is not a Ewallet.
        Integer notEwalletObject = Integer.valueOf(1);
        assertThat(ewallet1.equals((Object) notEwalletObject)).isFalse();
    }

    @Test
    public void eWallet_noEwalletName() {
        AssertionError error =
                assertThrows(
                        AssertionError.class,
                        () ->
                                new Ewallet.Builder()
                                        .setAccountDisplayName("account display name")
                                        .setPaymentInstrument(
                                                new PaymentInstrument.Builder()
                                                        .setInstrumentId(100)
                                                        .setNickname("nickname")
                                                        .setDisplayIconUrl(
                                                                new GURL("http://www.example.com"))
                                                        .setSupportedPaymentRails(new int[] {2})
                                                        .setIsFidoEnrolled(true)
                                                        .build())
                                        .build());

        assertThat(error.getMessage()).isEqualTo("Ewallet name cannot be null or empty.");
    }

    @Test
    public void eWallet_noAccountDisplayName() {
        AssertionError error =
                assertThrows(
                        AssertionError.class,
                        () ->
                                new Ewallet.Builder()
                                        .setEwalletName("ewallet name")
                                        .setPaymentInstrument(
                                                new PaymentInstrument.Builder()
                                                        .setInstrumentId(100)
                                                        .setNickname("nickname")
                                                        .setDisplayIconUrl(
                                                                new GURL("http://www.example.com"))
                                                        .setSupportedPaymentRails(new int[] {2})
                                                        .setIsFidoEnrolled(true)
                                                        .build())
                                        .build());

        assertThat(error.getMessage()).isEqualTo("Account display name cannot be null or empty.");
    }
}
