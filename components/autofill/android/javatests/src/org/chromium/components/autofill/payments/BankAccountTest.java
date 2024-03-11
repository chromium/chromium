// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.payments;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.url.GURL;

/** Unit tests for {@link BankAccount} */
@RunWith(BaseRobolectricTestRunner.class)
public class BankAccountTest {

    @Test
    public void bankAccount_build() {
        BankAccount bankAccount =
                new BankAccount.Builder()
                        .setBankName("bank name")
                        .setAccountNumberSuffix("account number suffix")
                        .setAccountType(1)
                        .setPaymentInstrument(
                                new PaymentInstrument.Builder()
                                        .setInstrumentId(100)
                                        .setNickname("nickname")
                                        .setDisplayIconUrl(new GURL("http://www.example.com"))
                                        .setSupportedPaymentRails(new int[] {1})
                                        .build())
                        .build();

        assertThat(bankAccount.getAccountNumberSuffix()).isEqualTo("account number suffix");
        assertThat(bankAccount.getBankName()).isEqualTo("bank name");
        assertThat(bankAccount.getAccountType()).isEqualTo(1);
        assertThat(bankAccount.getInstrumentId()).isEqualTo(100);
        assertThat(bankAccount.getNickname()).isEqualTo("nickname");
        assertThat(bankAccount.getDisplayIconUrl()).isEqualTo(new GURL("http://www.example.com"));
        assertThat(bankAccount.getSupportedPaymentRails()).isEqualTo(new int[] {1});
    }

    @Test
    public void bankAccount_create() {
        BankAccount bankAccount =
                BankAccount.create(
                        /* instrumentId= */ 100,
                        "nickname",
                        new GURL("http://www.example.com"),
                        /* supportedPaymentRails= */ new int[] {1},
                        "bank name",
                        "account number suffix",
                        /* accountType= */ 1);

        assertThat(bankAccount.getAccountNumberSuffix()).isEqualTo("account number suffix");
        assertThat(bankAccount.getBankName()).isEqualTo("bank name");
        assertThat(bankAccount.getAccountType()).isEqualTo(1);
        assertThat(bankAccount.getInstrumentId()).isEqualTo(100);
        assertThat(bankAccount.getNickname()).isEqualTo("nickname");
        assertThat(bankAccount.getDisplayIconUrl()).isEqualTo(new GURL("http://www.example.com"));
        assertThat(bankAccount.getSupportedPaymentRails()).isEqualTo(new int[] {1});
    }
}
