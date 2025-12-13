// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.payments;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.contains;
import static org.hamcrest.Matchers.equalTo;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.autofill.R;

import java.util.Arrays;

@RunWith(BaseRobolectricTestRunner.class)
public class BnplIssuerTosDetailTest {
    private static final String ISSUER_ID = "affirm";
    private static final String ISSUER_NAME = "Affirm";
    private static final LegalMessageLine LEGAL_MESSAGE_LINE =
            new LegalMessageLine("Legal message line");

    @Test
    public void bnplIssuerTosDetail_constructor_setsProperties() {
        BnplIssuerTosDetail bnplIssuerTosDetail =
                new BnplIssuerTosDetail(
                        /* issuerId= */ ISSUER_ID,
                        /* headerIconDrawableId= */ R.drawable.bnpl_icon_generic,
                        /* headerIconDarkDrawableId= */ R.drawable.error_icon,
                        /* isLinkedIssuer= */ true,
                        /* issuerName= */ ISSUER_NAME,
                        Arrays.asList(LEGAL_MESSAGE_LINE));

        assertThat(bnplIssuerTosDetail.getIssuerId(), equalTo(ISSUER_ID));
        assertThat(
                bnplIssuerTosDetail.getHeaderIconDrawableId(),
                equalTo(R.drawable.bnpl_icon_generic));
        assertThat(
                bnplIssuerTosDetail.getHeaderIconDarkDrawableId(), equalTo(R.drawable.error_icon));
        assertTrue(bnplIssuerTosDetail.getIsLinkedIssuer());
        assertThat(bnplIssuerTosDetail.getIssuerName(), equalTo(ISSUER_NAME));
        assertThat(bnplIssuerTosDetail.getLegalMessageLines().size(), equalTo(1));
        assertThat(bnplIssuerTosDetail.getLegalMessageLines(), contains(LEGAL_MESSAGE_LINE));
    }
}
