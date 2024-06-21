// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.payments;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.equalTo;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

@RunWith(BaseRobolectricTestRunner.class)
public class AutofillSaveIbanUiInfoTest {
    private static AutofillSaveIbanUiInfo.Builder defaultBuilder() {
        return new AutofillSaveIbanUiInfo.Builder()
                .withAcceptText("")
                .withCancelText("")
                .withIbanLabel("")
                .withTitleText("");
    }

    @Test
    public void testBuilder_setsAcceptText() {
        AutofillSaveIbanUiInfo uiInfo = defaultBuilder().withAcceptText("Save").build();

        assertThat(uiInfo.getAcceptText(), equalTo("Save"));
    }

    @Test
    public void testBuilder_setsCancelText() {
        AutofillSaveIbanUiInfo uiInfo = defaultBuilder().withCancelText("No thanks").build();

        assertThat(uiInfo.getCancelText(), equalTo("No thanks"));
    }

    @Test
    public void testBuilder_setsIbanLabel() {
        AutofillSaveIbanUiInfo uiInfo =
                defaultBuilder().withIbanLabel("FR** **** **** **** **** ***0 189").build();

        assertThat(uiInfo.getIbanLabel(), equalTo("FR** **** **** **** **** ***0 189"));
    }

    @Test
    public void testBuilder_setsTitleText() {
        AutofillSaveIbanUiInfo uiInfo = defaultBuilder().withTitleText("Save IBAN?").build();

        assertThat(uiInfo.getTitleText(), equalTo("Save IBAN?"));
    }
}
