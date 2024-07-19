// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.payments;

import static com.google.common.truth.Truth.assertThat;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.equalTo;
import static org.junit.Assert.assertThrows;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

@RunWith(BaseRobolectricTestRunner.class)
public class AutofillSaveIbanUiInfoTest {
    private static AutofillSaveIbanUiInfo.Builder defaultBuilder() {
        return new AutofillSaveIbanUiInfo.Builder()
                .withAcceptText("")
                .withCancelText("")
                .withIbanLabel("FR76 3000 6000 0112 3456 7890 189")
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

    @Test
    public void uiInfo_noIbanLabel() {
        AssertionError error =
                assertThrows(
                        AssertionError.class,
                        () ->
                                new AutofillSaveIbanUiInfo.Builder()
                                        .withAcceptText("")
                                        .withCancelText("")
                                        .withTitleText("")
                                        .build());

        assertThat(error.getMessage()).isEqualTo("IBAN value cannot be null or empty.");
    }

    @Test
    public void uiInfo_emptyIbanLabel() {
        AssertionError error =
                assertThrows(
                        AssertionError.class,
                        () ->
                                new AutofillSaveIbanUiInfo.Builder()
                                        .withAcceptText("")
                                        .withCancelText("")
                                        .withIbanLabel("")
                                        .withTitleText("")
                                        .build());

        assertThat(error.getMessage()).isEqualTo("IBAN value cannot be null or empty.");
    }
}
