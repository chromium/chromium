// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.payments;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.equalTo;
import static org.mockito.Mockito.mock;

import android.text.SpannableString;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.autofill.R;

import java.util.Arrays;
import java.util.function.Consumer;

@RunWith(BaseRobolectricTestRunner.class)
public class BnplIssuerTosDetailTest {
    private static final Consumer<String> MOCK_LINK_OPENER = mock(Consumer.class);

    @Test
    public void bnplIssuerTosDetail_constructor_setsProperties() {
        BnplIssuerTosDetail bnplIssuerTosDetail =
                new BnplIssuerTosDetail(
                        /* headerIconDrawableId= */ R.drawable.bnpl_icon_generic,
                        /* headerIconDarkDrawableId= */ R.drawable.error_icon,
                        "title text",
                        "review text",
                        "approve text",
                        new SpannableString("link text"),
                        new BnplIssuerTosDetail.LegalMessages(
                                Arrays.asList(new LegalMessageLine("Legal message line")),
                                MOCK_LINK_OPENER));

        assertThat(
                bnplIssuerTosDetail.getHeaderIconDrawableId(),
                equalTo(R.drawable.bnpl_icon_generic));
        assertThat(
                bnplIssuerTosDetail.getHeaderIconDarkDrawableId(), equalTo(R.drawable.error_icon));
        assertThat(bnplIssuerTosDetail.getTitle(), equalTo("title text"));
        assertThat(bnplIssuerTosDetail.getReviewText(), equalTo("review text"));
        assertThat(bnplIssuerTosDetail.getApproveText(), equalTo("approve text"));
        assertThat(bnplIssuerTosDetail.getLinkText().toString(), equalTo("link text"));
        BnplIssuerTosDetail.LegalMessages legalMessages = bnplIssuerTosDetail.getLegalMessages();
        assertThat(legalMessages.mLines.size(), equalTo(1));
        assertThat(legalMessages.mLines.get(0).text, equalTo("Legal message line"));
        assertThat(legalMessages.mLinkOpener, equalTo(MOCK_LINK_OPENER));
    }
}
