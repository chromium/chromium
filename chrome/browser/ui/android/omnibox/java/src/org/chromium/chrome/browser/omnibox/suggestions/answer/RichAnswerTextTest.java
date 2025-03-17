// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.answer;

import android.app.Activity;
import android.content.Context;
import android.text.SpannableStringBuilder;
import android.text.style.TextAppearanceSpan;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.components.omnibox.AnswerDataProto.AnswerData;
import org.chromium.components.omnibox.AnswerDataProto.FormattedString;
import org.chromium.components.omnibox.AnswerDataProto.FormattedString.ColorType;
import org.chromium.components.omnibox.AnswerDataProto.FormattedString.FormattedStringFragment;
import org.chromium.components.omnibox.AnswerTypeProto.AnswerType;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.RichAnswerTemplateProto.RichAnswerTemplate;
import org.chromium.components.omnibox.RichAnswerTemplateProto.SuggestionEnhancement;
import org.chromium.components.omnibox.RichAnswerTemplateProto.SuggestionEnhancements;

/** Tests for {@link RichAnswerText}. */
@RunWith(BaseRobolectricTestRunner.class)
public class RichAnswerTextTest {
    private Context mContext;
    private TextAppearanceSpan mGreenText;
    private TextAppearanceSpan mRedText;
    private TextAppearanceSpan mPrimaryText;
    private TextAppearanceSpan mMediumText;
    private TextAppearanceSpan mHeadlineText;

    @Before
    public void setUp() {
        mContext = Robolectric.buildActivity(Activity.class).setup().get();
        mContext.setTheme(R.style.Theme_BrowserUI_DayNight);
        mGreenText =
                new TextAppearanceSpan(
                        mContext,
                        org.chromium.chrome.browser.omnibox.test.R.style
                                .TextAppearance_OmniboxAnswerDescriptionPositive);
        mRedText =
                new TextAppearanceSpan(
                        mContext,
                        org.chromium.chrome.browser.omnibox.test.R.style
                                .TextAppearance_OmniboxAnswerDescriptionNegative);
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
        mHeadlineText =
                new TextAppearanceSpan(
                        mContext,
                        org.chromium.chrome.browser.omnibox.R.style
                                .TextAppearance_Headline2Thick_Primary);
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
                        .addAnswers(
                                0,
                                AnswerData.newBuilder().setHeadline(headline).setSubhead(subhead))
                        .build();

        AnswerType answerType = AnswerType.ANSWER_TYPE_DICTIONARY;
        AnswerText[] texts =
                RichAnswerText.from(mContext, richAnswerTemplate, answerType, false, false);
        Assert.assertEquals(1, texts[0].getMaxLines());
        Assert.assertEquals(3, texts[1].getMaxLines());
        Assert.assertEquals("define adroit • /əˈdroit/", texts[0].getAccessibilityDescription());
        Assert.assertEquals(
                "clever or skillful in using the hands or mind.",
                texts[1].getAccessibilityDescription());

        SpannableStringBuilder primaryText = texts[0].getText();
        SpannableStringBuilder secondaryText = texts[1].getText();

        Assert.assertEquals("define adroit • /əˈdroit/", primaryText.toString());
        TextAppearanceSpan[] textAppearanceSpans =
                primaryText.getSpans(0, primaryText.length(), TextAppearanceSpan.class);
        Assert.assertEquals(1, textAppearanceSpans.length);
        Assert.assertEquals(textAppearanceSpans[0].getTextSize(), mPrimaryText.getTextSize());

        Assert.assertEquals(
                "clever or skillful in using the hands or mind.", secondaryText.toString());
        textAppearanceSpans =
                secondaryText.getSpans(0, secondaryText.length(), TextAppearanceSpan.class);
        Assert.assertEquals(1, textAppearanceSpans.length);
        Assert.assertEquals(textAppearanceSpans[0].getTextSize(), mMediumText.getTextSize());
    }

    @Test
    @SmallTest
    @DisableFeatures(OmniboxFeatureList.OMNIBOX_ANSWER_ACTIONS)
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
                        .addAnswers(
                                0,
                                AnswerData.newBuilder()
                                        .setHeadline(headline)
                                        .setSubhead(positiveSubhead))
                        .build();

        AnswerType answerType = AnswerType.ANSWER_TYPE_FINANCE;
        AnswerText[] texts =
                RichAnswerText.from(mContext, richAnswerTemplate, answerType, false, false);
        // A11y descriptions are reverse of visual ordering.
        Assert.assertEquals(
                "goog stock GOOG(NASDAQ), 3:22 PM EDT", texts[0].getAccessibilityDescription());
        Assert.assertEquals("100.00 +1.00", texts[1].getAccessibilityDescription());
        SpannableStringBuilder primaryText = texts[0].getText();
        SpannableStringBuilder secondaryText = texts[1].getText();

        Assert.assertEquals("100.00 +1.00", primaryText.toString());
        TextAppearanceSpan[] textAppearanceSpans =
                primaryText.getSpans(0, primaryText.length(), TextAppearanceSpan.class);
        Assert.assertEquals(2, textAppearanceSpans.length);
        Assert.assertEquals(1, texts[0].getMaxLines());
        Assert.assertEquals(1, texts[1].getMaxLines());
        Assert.assertEquals(textAppearanceSpans[0].getTextSize(), mPrimaryText.getTextSize());
        Assert.assertEquals(textAppearanceSpans[1].getTextSize(), mGreenText.getTextSize());
        Assert.assertEquals(textAppearanceSpans[1].getTextColor(), mGreenText.getTextColor());

        Assert.assertEquals("goog stock GOOG(NASDAQ), 3:22 PM EDT", secondaryText.toString());
        textAppearanceSpans =
                secondaryText.getSpans(0, secondaryText.length(), TextAppearanceSpan.class);
        Assert.assertEquals(1, textAppearanceSpans.length);
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
                        .addAnswers(
                                0,
                                AnswerData.newBuilder()
                                        .setHeadline(headline)
                                        .setSubhead(negativeSubhead))
                        .build();
        texts = RichAnswerText.from(mContext, negativeRichAnswerTemplate, answerType, false, false);
        primaryText = texts[0].getText();

        Assert.assertEquals("100.00 -1.00", primaryText.toString());
        textAppearanceSpans =
                primaryText.getSpans(0, primaryText.length(), TextAppearanceSpan.class);
        Assert.assertEquals(2, textAppearanceSpans.length);
        Assert.assertEquals(textAppearanceSpans[0].getTextSize(), mPrimaryText.getTextSize());
        Assert.assertEquals(textAppearanceSpans[1].getTextSize(), mRedText.getTextSize());
        Assert.assertEquals(textAppearanceSpans[1].getTextColor(), mRedText.getTextColor());
    }

    @Test
    @SmallTest
    @DisableFeatures(OmniboxFeatureList.OMNIBOX_ANSWER_ACTIONS)
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
                        .addAnswers(
                                0,
                                AnswerData.newBuilder()
                                        .setHeadline(headline)
                                        .setSubhead(positiveSubhead))
                        .build();

        AnswerType answerType = AnswerType.ANSWER_TYPE_FINANCE;
        AnswerText[] texts =
                RichAnswerText.from(mContext, richAnswerTemplate, answerType, true, false);
        SpannableStringBuilder primaryText = texts[0].getText();

        Assert.assertEquals("100.00 +1.00", primaryText.toString());
        TextAppearanceSpan[] textAppearanceSpans =
                primaryText.getSpans(0, primaryText.length(), TextAppearanceSpan.class);
        Assert.assertEquals(2, textAppearanceSpans.length);
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
                        .addAnswers(
                                0,
                                AnswerData.newBuilder()
                                        .setHeadline(headline)
                                        .setSubhead(negativeSubhead))
                        .build();

        texts = RichAnswerText.from(mContext, negativeRichAnswerTemplate, answerType, true, false);
        primaryText = texts[0].getText();

        Assert.assertEquals("100.00 -1.00", primaryText.toString());
        textAppearanceSpans =
                primaryText.getSpans(0, primaryText.length(), TextAppearanceSpan.class);
        Assert.assertEquals(2, textAppearanceSpans.length);
        Assert.assertEquals(textAppearanceSpans[1].getTextSize(), mGreenText.getTextSize());
        Assert.assertEquals(textAppearanceSpans[1].getTextColor(), mGreenText.getTextColor());
    }

    @Test
    @SmallTest
    @DisableFeatures(OmniboxFeatureList.OMNIBOX_ANSWER_ACTIONS)
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
                        .addAnswers(
                                0,
                                AnswerData.newBuilder().setHeadline(headline).setSubhead(subhead))
                        .build();

        AnswerType answerType = AnswerType.ANSWER_TYPE_WEATHER;
        AnswerText[] texts =
                RichAnswerText.from(mContext, richAnswerTemplate, answerType, false, false);
        Assert.assertEquals(1, texts[0].getMaxLines());
        Assert.assertEquals(1, texts[1].getMaxLines());

        SpannableStringBuilder primaryText = texts[0].getText();
        SpannableStringBuilder secondaryText = texts[1].getText();

        Assert.assertEquals("64•F Thu - Redmond, WA", primaryText.toString());
        TextAppearanceSpan[] textAppearanceSpans =
                primaryText.getSpans(0, primaryText.length(), TextAppearanceSpan.class);
        Assert.assertEquals(1, textAppearanceSpans.length);
        Assert.assertEquals(textAppearanceSpans[0].getTextSize(), mPrimaryText.getTextSize());

        Assert.assertEquals("redmond weather", secondaryText.toString());
        textAppearanceSpans =
                secondaryText.getSpans(0, secondaryText.length(), TextAppearanceSpan.class);
        Assert.assertEquals(1, textAppearanceSpans.length);
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
                        .addAnswers(
                                0,
                                AnswerData.newBuilder().setHeadline(headline).setSubhead(subhead))
                        .build();

        AnswerType answerType = AnswerType.ANSWER_TYPE_TRANSLATION;
        AnswerText[] texts =
                RichAnswerText.from(mContext, richAnswerTemplate, answerType, false, false);
        Assert.assertEquals(3, texts[0].getMaxLines());
        Assert.assertEquals(1, texts[1].getMaxLines());
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
                        .addAnswers(
                                0,
                                AnswerData.newBuilder().setHeadline(headline).setSubhead(subhead))
                        .build();

        AnswerType answerType = AnswerType.ANSWER_TYPE_CURRENCY;
        AnswerText[] texts =
                RichAnswerText.from(mContext, richAnswerTemplate, answerType, false, false);
        SpannableStringBuilder primaryText = texts[0].getText();
        SpannableStringBuilder secondaryText = texts[1].getText();

        Assert.assertEquals("156.23 Japanese Yen", primaryText.toString());
        TextAppearanceSpan[] textAppearanceSpans =
                primaryText.getSpans(0, primaryText.length(), TextAppearanceSpan.class);
        Assert.assertEquals(1, textAppearanceSpans.length);
        Assert.assertEquals(textAppearanceSpans[0].getTextSize(), mPrimaryText.getTextSize());

        Assert.assertEquals("1 usd to jpy", secondaryText.toString());
    }

    @Test
    @SmallTest
    @DisableFeatures(OmniboxFeatureList.OMNIBOX_ANSWER_ACTIONS)
    public void testNoFragments() {
        FormattedString headline = FormattedString.newBuilder().setText("redmond weather").build();
        FormattedString subhead =
                FormattedString.newBuilder().setText("64•F Thu - Redmond, WA").build();

        RichAnswerTemplate richAnswerTemplate =
                RichAnswerTemplate.newBuilder()
                        .addAnswers(
                                0,
                                AnswerData.newBuilder().setHeadline(headline).setSubhead(subhead))
                        .build();

        AnswerType answerType = AnswerType.ANSWER_TYPE_WEATHER;
        AnswerText[] texts =
                RichAnswerText.from(mContext, richAnswerTemplate, answerType, false, false);
        SpannableStringBuilder primaryText = texts[0].getText();
        SpannableStringBuilder secondaryText = texts[1].getText();

        Assert.assertEquals("64•F Thu - Redmond, WA", primaryText.toString());
        TextAppearanceSpan[] textAppearanceSpans =
                primaryText.getSpans(0, primaryText.length(), TextAppearanceSpan.class);
        Assert.assertEquals(1, textAppearanceSpans.length);
        Assert.assertEquals(textAppearanceSpans[0].getTextSize(), mPrimaryText.getTextSize());

        Assert.assertEquals("redmond weather", secondaryText.toString());
        textAppearanceSpans =
                secondaryText.getSpans(0, secondaryText.length(), TextAppearanceSpan.class);
        Assert.assertEquals(1, textAppearanceSpans.length);
        Assert.assertEquals(textAppearanceSpans[0].getTextSize(), mMediumText.getTextSize());
    }

    @Test
    @SmallTest
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_ANSWER_ACTIONS)
    public void testRichAnswerCard() {
        OmniboxFeatures.sAnswerActionsShowRichCard.setForTesting(true);
        // The backend sends the lines in Answer > query order for some answer types (dictionary,
        // sports, weather, finance, knowledge graph). These should not have their order reversed.
        FormattedString headline =
                FormattedString.newBuilder().setText("64•F Thu - Redmond, WA").build();
        FormattedString subhead = FormattedString.newBuilder().setText("redmond weather").build();

        RichAnswerTemplate richAnswerTemplate =
                RichAnswerTemplate.newBuilder()
                        .setEnhancements(
                                SuggestionEnhancements.newBuilder()
                                        .addEnhancements(
                                                SuggestionEnhancement.newBuilder()
                                                        .setDisplayText("7 day forecast"))
                                        .build())
                        .addAnswers(
                                0,
                                AnswerData.newBuilder().setHeadline(headline).setSubhead(subhead))
                        .build();

        AnswerType answerType = AnswerType.ANSWER_TYPE_WEATHER;
        AnswerText[] texts =
                RichAnswerText.from(mContext, richAnswerTemplate, answerType, false, true);
        SpannableStringBuilder primaryText = texts[0].getText();
        SpannableStringBuilder secondaryText = texts[1].getText();

        Assert.assertEquals("64•F Thu - Redmond, WA", primaryText.toString());
        TextAppearanceSpan[] textAppearanceSpans =
                primaryText.getSpans(0, primaryText.length(), TextAppearanceSpan.class);
        Assert.assertEquals(1, textAppearanceSpans.length);
        Assert.assertEquals(textAppearanceSpans[0].getTextSize(), mHeadlineText.getTextSize());

        Assert.assertEquals("redmond weather", secondaryText.toString());
        textAppearanceSpans =
                secondaryText.getSpans(0, secondaryText.length(), TextAppearanceSpan.class);
        Assert.assertEquals(1, textAppearanceSpans.length);
        Assert.assertEquals(textAppearanceSpans[0].getTextSize(), mPrimaryText.getTextSize());
    }
}
