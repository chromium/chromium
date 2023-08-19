// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.payments;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.equalTo;

import android.annotation.SuppressLint;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

@RunWith(BaseRobolectricTestRunner.class)
public class CardDetailTest {
    @Test
    public void cardDetailConstructor_setsProperties() {
        @SuppressLint("ResourceType")
        CardDetail cardDetail = new CardDetail(1, "2", "3");

        assertThat(cardDetail.issuerIconDrawableId, equalTo(1));
        assertThat(cardDetail.label, equalTo("2"));
        assertThat(cardDetail.subLabel, equalTo("3"));
    }
}
