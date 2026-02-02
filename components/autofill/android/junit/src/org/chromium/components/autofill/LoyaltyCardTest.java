// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.equalTo;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link LoyaltyCard}. */
@RunWith(BaseRobolectricTestRunner.class)
public class LoyaltyCardTest {
    @Test
    public void testLoyaltyCardConstructor_setsProperties() {
        String loyaltyCardId = "card_id";
        String merchantName = "merchant";
        String programName = "program";
        GURL programLogo = new GURL("https://example.com/logo.png");
        String loyaltyCardNumber = "123456";
        List<GURL> merchantDomains = Arrays.asList(new GURL("https://example.com"));
        long useDate = 123456789L;
        long useCount = 5;

        LoyaltyCard card =
                new LoyaltyCard(
                        loyaltyCardId,
                        merchantName,
                        programName,
                        programLogo,
                        loyaltyCardNumber,
                        merchantDomains,
                        useDate,
                        useCount);

        assertThat(card.getLoyaltyCardId(), equalTo(loyaltyCardId));
        assertThat(card.getMerchantName(), equalTo(merchantName));
        assertThat(card.getProgramName(), equalTo(programName));
        assertThat(card.getProgramLogo(), equalTo(programLogo));
        assertThat(card.getLoyaltyCardNumber(), equalTo(loyaltyCardNumber));
        assertThat(card.getMerchantDomains(), equalTo(merchantDomains));
        assertThat(card.getUseDate(), equalTo(useDate));
        assertThat(card.getUseCount(), equalTo(useCount));
    }
}
