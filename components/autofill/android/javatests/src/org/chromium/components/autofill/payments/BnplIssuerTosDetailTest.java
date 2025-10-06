// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.payments;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.equalTo;

import android.text.SpannableString;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

@RunWith(BaseRobolectricTestRunner.class)
public class BnplIssuerTosDetailTest {
    @Test
    public void bnplIssuerTosDetail_constructor_setsProperties() {
        BnplIssuerTosDetail bnplIssuerTosDetail =
                new BnplIssuerTosDetail(
                        "review text", "approve text", new SpannableString("link text"));

        assertThat(bnplIssuerTosDetail.getReviewText(), equalTo("review text"));
        assertThat(bnplIssuerTosDetail.getApproveText(), equalTo("approve text"));
        assertThat(bnplIssuerTosDetail.getLinkText().toString(), equalTo("link text"));
    }
}
