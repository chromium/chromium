// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.payments;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.equalTo;

import android.annotation.SuppressLint;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.Matchers;
import org.hamcrest.TypeSafeMatcher;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Objects;
import java.util.stream.Collectors;

@RunWith(BaseRobolectricTestRunner.class)
public class AutofillSaveCardUiInfoTest {
    private static AutofillSaveCardUiInfo.Builder defaultBuilder() {
        return new AutofillSaveCardUiInfo.Builder()
                .withLogoIcon(0)
                .withIsForUpload(false)
                .withCardDetail(new CardDetail(0, "", ""))
                .withCardDescription("")
                .withLegalMessageLines(Collections.EMPTY_LIST)
                .withTitleText("")
                .withConfirmText("")
                .withCancelText("")
                .withDescriptionText("")
                .withLoadingDescription("")
                .withIsGooglePayBrandingEnabled(false);
    }

    @Test
    public void testBuilder_setsIsForUpload() {
        AutofillSaveCardUiInfo uiInfo = defaultBuilder().withIsForUpload(true).build();

        assertThat(uiInfo.isForUpload(), equalTo(true));
    }

    @Test
    public void testBuilder_setsLogoIcon() {
        @SuppressLint("ResourceType")
        AutofillSaveCardUiInfo uiInfo = defaultBuilder().withLogoIcon(1234).build();

        assertThat(uiInfo.getLogoIcon(), equalTo(1234));
    }

    @Test
    public void testBuilder_setsCardDetail() {
        @SuppressLint("ResourceType")
        AutofillSaveCardUiInfo uiInfo =
                defaultBuilder()
                        .withCardDetail(
                                new CardDetail(/* iconId= */ 1, "cardLabel", "cardSubLabel"))
                        .build();

        assertThat(uiInfo.getCardDetail().issuerIconDrawableId, equalTo(1));
        assertThat(uiInfo.getCardDetail().label, equalTo("cardLabel"));
        assertThat(uiInfo.getCardDetail().subLabel, equalTo("cardSubLabel"));
    }

    @Test
    public void testBuilder_setsCardDescription() {
        AutofillSaveCardUiInfo uiInfo =
                defaultBuilder().withCardDescription("cardDescription").build();

        assertThat(uiInfo.getCardDescription(), equalTo("cardDescription"));
    }

    @Test
    public void testBuilder_setsLegalMessageLine() {
        List<LegalMessageLine> legalMessageLines =
                Arrays.asList(
                        new LegalMessageLine("abc"),
                        new LegalMessageLine("xyz"),
                        new LegalMessageLine("uvw"));

        AutofillSaveCardUiInfo uiInfo =
                defaultBuilder().withLegalMessageLines(legalMessageLines).build();

        assertThat(
                uiInfo.getLegalMessageLines(),
                Matchers.contains(
                        legalMessageLines.stream()
                                .map(line -> equalToLegalMessageLineWithText(line.text))
                                .collect(Collectors.toList())));
    }

    private static Matcher<LegalMessageLine> equalToLegalMessageLineWithText(String text) {
        return new TypeSafeMatcher<LegalMessageLine>() {
            @Override
            protected boolean matchesSafely(LegalMessageLine legalMessageLine) {
                return Objects.equals(legalMessageLine.text, text);
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("LegalLineMessage with text of ").appendValue(text);
            }

            @Override
            protected void describeMismatchSafely(
                    LegalMessageLine item, Description mismatchDescription) {
                mismatchDescription
                        .appendText("LegalLineMessage with text of ")
                        .appendValue(item.text);
            }
        };
    }

    @Test
    public void testBuilder_setsTitleText() {
        AutofillSaveCardUiInfo uiInfo = defaultBuilder().withTitleText("Title Text").build();

        assertThat(uiInfo.getTitleText(), equalTo("Title Text"));
    }

    @Test
    public void testBuilder_setsConfirmText() {
        AutofillSaveCardUiInfo uiInfo = defaultBuilder().withConfirmText("Confirm Text").build();

        assertThat(uiInfo.getConfirmText(), equalTo("Confirm Text"));
    }

    @Test
    public void testBuilder_setsCancelText() {
        AutofillSaveCardUiInfo uiInfo = defaultBuilder().withCancelText("Cancel Text").build();

        assertThat(uiInfo.getCancelText(), equalTo("Cancel Text"));
    }

    @Test
    public void testBuilder_setsDescriptionText() {
        AutofillSaveCardUiInfo uiInfo =
                defaultBuilder().withDescriptionText("Description Text").build();

        assertThat(uiInfo.getDescriptionText(), equalTo("Description Text"));
    }

    @Test
    public void testBuilder_setsLoadingDescription() {
        AutofillSaveCardUiInfo uiInfo =
                defaultBuilder().withLoadingDescription("Loading Description").build();

        assertThat(uiInfo.getLoadingDescription(), equalTo("Loading Description"));
    }

    @Test
    public void testBuilder_setsGooglePayBrandingEnabled() {
        AutofillSaveCardUiInfo uiInfo =
                defaultBuilder().withIsGooglePayBrandingEnabled(true).build();

        assertThat(uiInfo.isGooglePayBrandingEnabled(), equalTo(true));
    }
}
