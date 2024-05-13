// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.answer;

import android.content.Context;
import android.text.SpannableStringBuilder;
import android.text.style.TextAppearanceSpan;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.omnibox.AnswerDataProto.AnswerData;
import org.chromium.components.omnibox.AnswerDataProto.FormattedString;
import org.chromium.components.omnibox.AnswerDataProto.FormattedString.ColorType;
import org.chromium.components.omnibox.AnswerDataProto.FormattedString.FormattedStringFragment;
import org.chromium.components.omnibox.RichAnswerTemplateProto.RichAnswerTemplate;
import org.chromium.components.omnibox.RichAnswerTemplateProto.RichAnswerTemplate.AnswerType;

/** Tests for {@link RichAnswerText}. */
@RunWith(BaseRobolectricTestRunner.class)
public class RichAnswerTextTest {
    private Context mContext;
    private TextAppearanceSpan mGreenText;
    private TextAppearanceSpan mRedText;
    private TextAppearanceSpan mPrimaryText;
    private TextAppearanceSpan mMediumText;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
        mGreenText =
                new TextAppearanceSpan(
                        mContext,
                        org.chromium.chrome.browser.omnibox.test.R.style
                                .TextAppearance_OmniboxAnswerDescriptionPositiveSmall);
        mRedText =
                new TextAppearanceSpan(
                        mContext,
                        org.chromium.chrome.browser.omnibox.test.R.style
                                .TextAppearance_OmniboxAnswerDescriptionNegativeSmall);
        mPrimaryText =
                new TextAppearanceSpan(
                        mContext,
                        org.chromium.chrome.browser.omnibox.R.style
                                .TextAppearance_TextLarge_Primary);
        mMediumText =
                new TextAppearanceSpan(
                        mContext,
                        org.chromium.chrome.browser.omnibox.R.style
                                .TextAppearance_TextMedium_Secondary);
    }

    @Test
    @SmallTest
    public void testDictionaryAnswer() {
        FormattedString headline =
                FormattedString.newBuilder()
                        .addFragments(
                                FormattedStringFragment.newBuilder()
                                        .setStartIndex(0)
                                        .setText("define adroit • /əˈdroit/"))
                        .build();
        FormattedString subhead =
                FormattedString.newBuilder()
                        .addFragments(
                                FormattedStringFragment.newBuilder()
                                        .setStartIndex(0)
                                        .setText("clever or skillful in using the hands or mind."))
                        .build();

        RichAnswerTemplate richAnswerTemplate =
                RichAnswerTemplate.newBuilder()
                        .setAnswerType(AnswerType.DICTIONARY)
                        .addAnswers(
                                0,
                                AnswerData.newBuilder().setHeadline(headline).setSubhead(subhead))
                        .build();

        AnswerText[] texts = RichAnswerText.from(mContext, richAnswerTemplate, false);
        Assert.assertEquals(texts[0].getMaxLines(), 1);
        Assert.assertEquals(texts[1].getMaxLines(), 1);
        Assert.assertEquals(texts[0].getAccessibilityDescription(), "define adroit • /əˈdroit/");
        Assert.assertEquals(
                texts[1].getAccessibilityDescription(),
                "clever or skillful in using the hands or mind.");

        SpannableStringBuilder primaryText = texts[0].getText();
        SpannableStringBuilder secondaryText = texts[1].getText();

        Assert.assertEquals(primaryText.toString(), "define adroit • /əˈdroit/");
        TextAppearanceSpan[] textAppearanceSpans =
                primaryText.getSpans(0, primaryText.length(), TextAppearanceSpan.class);
        Assert.assertEquals(textAppearanceSpans.length, 1);
        Assert.assertEquals(textAppearanceSpans[0].getTextSize(), mPrimaryText.getTextSize());

        Assert.assertEquals(
                secondaryText.toString(), "clever or skillful in using the hands or mind.");
        textAppearanceSpans =
                secondaryText.getSpans(0, secondaryText.length(), TextAppearanceSpan.class);
        Assert.assertEquals(textAppearanceSpans.length, 1);
        Assert.assertEquals(textAppearanceSpans[0].getTextSize(), mMediumText.getTextSize());
    }

    @Test
    @SmallTest
    public void testFinanceAnswer() {
        FormattedString headline =
                FormattedString.newBuilder()
                        .addFragments(
                                FormattedStringFragment.newBuilder()
                                        .setStartIndex(0)
                                        .setText("goog stock GOOG(NASDAQ), 3:22 PM EDT"))
                        .build();
        FormattedString positiveSubhead =
                FormattedString.newBuilder()
                        .addFragments(
                                FormattedStringFragment.newBuilder()
                                        .setStartIndex(0)
                                        .setText("100.00"))
                        .addFragments(
                                FormattedStringFragment.newBuilder()
                                        .setStartIndex(0)
                                        .setText("+1.00")
                                        .setColor(ColorType.COLOR_ON_SURFACE_POSITIVE))
                        .build();

        RichAnswerTemplate richAnswerTemplate =
                RichAnswerTemplate.newBuilder()
                        .setAnswerType(AnswerType.FINANCE)
                        .addAnswers(
                                0,
                                AnswerData.newBuilder()
                                        .setHeadline(headline)
                                        .setSubhead(positiveSubhead))
                        .build();

        AnswerText[] texts = RichAnswerText.from(mContext, richAnswerTemplate, false);
        // A11y descriptions are reverse of visual ordering.
        Assert.assertEquals(
                texts[0].getAccessibilityDescription(), "goog stock GOOG(NASDAQ), 3:22 PM EDT");
        Assert.assertEquals(texts[1].getAccessibilityDescription(), "100.00 +1.00");
        SpannableStringBuilder primaryText = texts[0].getText();
        SpannableStringBuilder secondaryText = texts[1].getText();

        Assert.assertEquals(primaryText.toString(), "100.00 +1.00");
        TextAppearanceSpan[] textAppearanceSpans =
                primaryText.getSpans(0, primaryText.length(), TextAppearanceSpan.class);
        Assert.assertEquals(textAppearanceSpans.length, 2);
        Assert.assertEquals(texts[0].getMaxLines(), 1);
        Assert.assertEquals(texts[1].getMaxLines(), 1);
        Assert.assertEquals(textAppearanceSpans[0].getTextSize(), mPrimaryText.getTextSize());
        Assert.assertEquals(textAppearanceSpans[1].getTextSize(), mGreenText.getTextSize());
        Assert.assertEquals(textAppearanceSpans[1].getTextColor(), mGreenText.getTextColor());

        Assert.assertEquals(secondaryText.toString(), "goog stock GOOG(NASDAQ), 3:22 PM EDT");
        textAppearanceSpans =
                secondaryText.getSpans(0, secondaryText.length(), TextAppearanceSpan.class);
        Assert.assertEquals(textAppearanceSpans.length, 1);
        Assert.assertEquals(textAppearanceSpans[0].getTextSize(), mMediumText.getTextSize());

        FormattedString negativeSubhead =
                FormattedString.newBuilder()
                        .addFragments(
                                FormattedStringFragment.newBuilder()
                                        .setStartIndex(0)
                                        .setText("100.00"))
                        .addFragments(
                                FormattedStringFragment.newBuilder()
                                        .setStartIndex(0)
                                        .setText("-1.00")
                                        .setColor(ColorType.COLOR_ON_SURFACE_NEGATIVE))
                        .build();

        RichAnswerTemplate negativeRichAnswerTemplate =
                RichAnswerTemplate.newBuilder()
                        .setAnswerType(AnswerType.FINANCE)
                        .addAnswers(
                                0,
                                AnswerData.newBuilder()
                                        .setHeadline(headline)
                                        .setSubhead(negativeSubhead))
                        .build();

        texts = RichAnswerText.from(mContext, negativeRichAnswerTemplate, false);
        primaryText = texts[0].getText();

        Assert.assertEquals(primaryText.toString(), "100.00 -1.00");
        textAppearanceSpans =
                primaryText.getSpans(0, primaryText.length(), TextAppearanceSpan.class);
        Assert.assertEquals(textAppearanceSpans.length, 2);
        Assert.assertEquals(textAppearanceSpans[0].getTextSize(), mPrimaryText.getTextSize());
        Assert.assertEquals(textAppearanceSpans[1].getTextSize(), mRedText.getTextSize());
        Assert.assertEquals(textAppearanceSpans[1].getTextColor(), mRedText.getTextColor());
    }

    @Test
    @SmallTest
    public void testFinanceAnswer_withColorReversal() {
        FormattedString headline =
                FormattedString.newBuilder()
                        .addFragments(
                                FormattedStringFragment.newBuilder()
                                        .setStartIndex(0)
                                        .setText("goog stock GOOG(NASDAQ), 3:22 PM EDT"))
                        .build();
        FormattedString positiveSubhead =
                FormattedString.newBuilder()
                        .addFragments(
                                FormattedStringFragment.newBuilder()
                                        .setStartIndex(0)
                                        .setText("100.00"))
                        .addFragments(
                                FormattedStringFragment.newBuilder()
                                        .setStartIndex(0)
                                        .setText("+1.00")
                                        .setColor(ColorType.COLOR_ON_SURFACE_POSITIVE))
                        .build();

        RichAnswerTemplate richAnswerTemplate =
                RichAnswerTemplate.newBuilder()
                        .setAnswerType(AnswerType.FINANCE)
                        .addAnswers(
                                0,
                                AnswerData.newBuilder()
                                        .setHeadline(headline)
                                        .setSubhead(positiveSubhead))
                        .build();

        AnswerText[] texts = RichAnswerText.from(mContext, richAnswerTemplate, true);
        SpannableStringBuilder primaryText = texts[0].getText();

        Assert.assertEquals(primaryText.toString(), "100.00 +1.00");
        TextAppearanceSpan[] textAppearanceSpans =
                primaryText.getSpans(0, primaryText.length(), TextAppearanceSpan.class);
        Assert.assertEquals(textAppearanceSpans.length, 2);
        Assert.assertEquals(textAppearanceSpans[1].getTextSize(), mRedText.getTextSize());
        Assert.assertEquals(textAppearanceSpans[1].getTextColor(), mRedText.getTextColor());

        FormattedString negativeSubhead =
                FormattedString.newBuilder()
                        .addFragments(
                                FormattedStringFragment.newBuilder()
                                        .setStartIndex(0)
                                        .setText("100.00"))
                        .addFragments(
                                FormattedStringFragment.newBuilder()
                                        .setStartIndex(0)
                                        .setText("-1.00")
                                        .setColor(ColorType.COLOR_ON_SURFACE_NEGATIVE))
                        .build();

        RichAnswerTemplate negativeRichAnswerTemplate =
                RichAnswerTemplate.newBuilder()
                        .setAnswerType(AnswerType.FINANCE)
                        .addAnswers(
                                0,
                                AnswerData.newBuilder()
                                        .setHeadline(headline)
                                        .setSubhead(negativeSubhead))
                        .build();

        texts = RichAnswerText.from(mContext, negativeRichAnswerTemplate, true);
        primaryText = texts[0].getText();

        Assert.assertEquals(primaryText.toString(), "100.00 -1.00");
        textAppearanceSpans =
                primaryText.getSpans(0, primaryText.length(), TextAppearanceSpan.class);
        Assert.assertEquals(textAppearanceSpans.length, 2);
        Assert.assertEquals(textAppearanceSpans[1].getTextSize(), mGreenText.getTextSize());
        Assert.assertEquals(textAppearanceSpans[1].getTextColor(), mGreenText.getTextColor());
    }

    @Test
    @SmallTest
    public void testWeatherAnswer() {
        FormattedString headline =
                FormattedString.newBuilder()
                        .addFragments(
                                FormattedStringFragment.newBuilder()
                                        .setStartIndex(0)
                                        .setText("redmond weather"))
                        .build();
        FormattedString subhead =
                FormattedString.newBuilder()
                        .addFragments(
                                FormattedStringFragment.newBuilder()
                                        .setStartIndex(0)
                                        .setText("64•F Thu - Redmond, WA"))
                        .build();

        RichAnswerTemplate richAnswerTemplate =
                RichAnswerTemplate.newBuilder()
                        .setAnswerType(AnswerType.WEATHER)
                        .addAnswers(
                                0,
                                AnswerData.newBuilder().setHeadline(headline).setSubhead(subhead))
                        .build();

        AnswerText[] texts = RichAnswerText.from(mContext, richAnswerTemplate, false);
        Assert.assertEquals(texts[0].getMaxLines(), 1);
        Assert.assertEquals(texts[1].getMaxLines(), 1);

        SpannableStringBuilder primaryText = texts[0].getText();
        SpannableStringBuilder secondaryText = texts[1].getText();

        Assert.assertEquals(primaryText.toString(), "64•F Thu - Redmond, WA");
        TextAppearanceSpan[] textAppearanceSpans =
                primaryText.getSpans(0, primaryText.length(), TextAppearanceSpan.class);
        Assert.assertEquals(textAppearanceSpans.length, 1);
        Assert.assertEquals(textAppearanceSpans[0].getTextSize(), mPrimaryText.getTextSize());

        Assert.assertEquals(secondaryText.toString(), "redmond weather");
        textAppearanceSpans =
                secondaryText.getSpans(0, secondaryText.length(), TextAppearanceSpan.class);
        Assert.assertEquals(textAppearanceSpans.length, 1);
        Assert.assertEquals(textAppearanceSpans[0].getTextSize(), mMediumText.getTextSize());
    }

    @Test
    @SmallTest
    public void testTranslationAnswer() {
        FormattedString headline =
                FormattedString.newBuilder()
                        .addFragments(
                                FormattedStringFragment.newBuilder()
                                        .setStartIndex(0)
                                        .setText("nuit (French)"))
                        .build();
        FormattedString subhead =
                FormattedString.newBuilder()
                        .addFragments(
                                FormattedStringFragment.newBuilder()
                                        .setStartIndex(0)
                                        .setText("night in French"))
                        .build();

        RichAnswerTemplate richAnswerTemplate =
                RichAnswerTemplate.newBuilder()
                        .setAnswerType(AnswerType.TRANSLATION)
                        .addAnswers(
                                0,
                                AnswerData.newBuilder().setHeadline(headline).setSubhead(subhead))
                        .build();

        AnswerText[] texts = RichAnswerText.from(mContext, richAnswerTemplate, false);
        Assert.assertEquals(texts[0].getMaxLines(), 3);
        Assert.assertEquals(texts[1].getMaxLines(), 1);
    }

    @Test
    @SmallTest
    public void testCurrencyAnswer() {
        FormattedString headline =
                FormattedString.newBuilder()
                        .addFragments(
                                FormattedStringFragment.newBuilder()
                                        .setStartIndex(0)
                                        .setText("1 usd to jpy"))
                        .build();
        FormattedString subhead =
                FormattedString.newBuilder()
                        .addFragments(
                                FormattedStringFragment.newBuilder()
                                        .setStartIndex(0)
                                        .setText("1 United States Dollar = 156.23 Japanese Yen"))
                        .build();

        RichAnswerTemplate richAnswerTemplate =
                RichAnswerTemplate.newBuilder()
                        .setAnswerType(AnswerType.CURRENCY)
                        .addAnswers(
                                0,
                                AnswerData.newBuilder().setHeadline(headline).setSubhead(subhead))
                        .build();

        AnswerText[] texts = RichAnswerText.from(mContext, richAnswerTemplate, false);
        SpannableStringBuilder primaryText = texts[0].getText();
        SpannableStringBuilder secondaryText = texts[1].getText();

        Assert.assertEquals(primaryText.toString(), "156.23 Japanese Yen");
        TextAppearanceSpan[] textAppearanceSpans =
                primaryText.getSpans(0, primaryText.length(), TextAppearanceSpan.class);
        Assert.assertEquals(textAppearanceSpans.length, 1);
        Assert.assertEquals(textAppearanceSpans[0].getTextSize(), mPrimaryText.getTextSize());

        Assert.assertEquals(secondaryText.toString(), "1 usd to jpy");
    }
}
