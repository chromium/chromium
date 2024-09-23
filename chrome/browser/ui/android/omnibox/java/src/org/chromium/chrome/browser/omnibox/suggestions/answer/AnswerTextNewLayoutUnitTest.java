// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.answer;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.core.IsInstanceOf.instanceOf;

import android.content.Context;
import android.content.res.ColorStateList;
import android.text.SpannableStringBuilder;
import android.text.style.MetricAffectingSpan;
import android.text.style.TextAppearanceSpan;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.test.R;
import org.chromium.components.omnibox.AnswerTextStyle;
import org.chromium.components.omnibox.AnswerTextType;
import org.chromium.components.omnibox.AnswerTypeProto.AnswerType;
import org.chromium.components.omnibox.SuggestionAnswer;
import org.chromium.components.omnibox.SuggestionAnswer.ImageLine;
import org.chromium.components.omnibox.SuggestionAnswer.TextField;

import java.util.List;

/** Tests for {@link AnswerTextNewLayout}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AnswerTextNewLayoutUnitTest {
    private Context mContext;
    private ColorStateList mGreenTextColor;
    private ColorStateList mRedTextColor;
    private TextAppearanceSpan mPrimaryText;
    private TextAppearanceSpan mSmallText;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
        mGreenTextColor =
                new TextAppearanceSpan(
                                mContext,
                                R.style.TextAppearance_OmniboxAnswerDescriptionPositiveSmall)
                        .getTextColor();
        mRedTextColor =
                new TextAppearanceSpan(
                                mContext,
                                R.style.TextAppearance_OmniboxAnswerDescriptionNegativeSmall)
                        .getTextColor();
        mPrimaryText =
                new TextAppearanceSpan(
                        mContext,
                        org.chromium.chrome.browser.omnibox.R.style
                                .TextAppearance_TextLarge_Primary);
        mSmallText =
                new TextAppearanceSpan(
                        mContext,
                        org.chromium.chrome.browser.omnibox.R.style
                                .TextAppearance_TextSmall_Secondary);
    }

    /** Check the validity of TextAppearanceSpan. */
    private void verifyTextAppearanceSpan(MetricAffectingSpan textAppearanceSpan) {
        assertThat(textAppearanceSpan, instanceOf(TextAppearanceSpan.class));
    }

    @Test
    @SmallTest
    public void testImageLineWithAddedTextFields() {
        TextField text =
                new TextField(AnswerTextType.SUGGESTION, "noun", AnswerTextStyle.NORMAL, 3);
        TextField additionalText =
                new TextField(
                        AnswerTextType.SUGGESTION_SECONDARY_TEXT_MEDIUM,
                        "verb",
                        AnswerTextStyle.SECONDARY,
                        1);
        TextField statusText =
                new TextField(AnswerTextType.SUGGESTION, "adverb", AnswerTextStyle.SUPERIOR, 1);
        SuggestionAnswer.ImageLine imageLine =
                new ImageLine(List.of(text), additionalText, statusText, "");
        AnswerTextNewLayout layout =
                new AnswerTextNewLayout(
                        mContext, AnswerType.ANSWER_TYPE_DICTIONARY, imageLine, true, false);

        Assert.assertEquals(layout.getText().toString(), "noun  verb  adverb");
        SpannableStringBuilder layoutText = layout.getText();
        TextAppearanceSpan[] textAppearanceSpans =
                layoutText.getSpans(0, layoutText.length(), TextAppearanceSpan.class);
        Assert.assertEquals(textAppearanceSpans.length, 3);
        Assert.assertEquals(textAppearanceSpans[0].getTextSize(), mPrimaryText.getTextSize());
        Assert.assertEquals(textAppearanceSpans[1].getTextSize(), mSmallText.getTextSize());
        Assert.assertEquals(textAppearanceSpans[2].getTextSize(), mPrimaryText.getTextSize());
    }

    /**
     * GetAppearanceForAnswerText should produce the correct color for stock ticker when the flag
     * for reversing Stock Ticker Color is disabled.
     */
    @Test
    @SmallTest
    public void getAppearanceForAnswerText_noColorReversal() {
        // Test for red text color.
        MetricAffectingSpan textAppearanceSpan1 =
                AnswerTextNewLayout.getAppearanceForAnswerText(
                        mContext,
                        AnswerTextType.DESCRIPTION_NEGATIVE,
                        AnswerType.ANSWER_TYPE_FINANCE,
                        false);
        verifyTextAppearanceSpan(textAppearanceSpan1);

        TextAppearanceSpan textAppearanceSpan1Converted = (TextAppearanceSpan) textAppearanceSpan1;
        Assert.assertEquals(mRedTextColor, textAppearanceSpan1Converted.getTextColor());

        // Test for green text color.
        MetricAffectingSpan textAppearanceSpan2 =
                AnswerTextNewLayout.getAppearanceForAnswerText(
                        mContext,
                        AnswerTextType.DESCRIPTION_POSITIVE,
                        AnswerType.ANSWER_TYPE_FINANCE,
                        false);
        verifyTextAppearanceSpan(textAppearanceSpan2);

        TextAppearanceSpan textAppearanceSpan2Converted = (TextAppearanceSpan) textAppearanceSpan2;
        Assert.assertEquals(mGreenTextColor, textAppearanceSpan2Converted.getTextColor());
    }

    /**
     * GetAppearanceForAnswerText should produce the reversed color for stock ticker when the flag
     * for reversing Stock Ticker Color is enabled.
     */
    @Test
    @SmallTest
    public void getAppearanceForAnswerText_withColorReversal() {
        // Test for green text color.
        MetricAffectingSpan textAppearanceSpan1 =
                AnswerTextNewLayout.getAppearanceForAnswerText(
                        mContext,
                        AnswerTextType.DESCRIPTION_NEGATIVE,
                        AnswerType.ANSWER_TYPE_FINANCE,
                        true);
        verifyTextAppearanceSpan(textAppearanceSpan1);

        TextAppearanceSpan textAppearanceSpan1Converted = (TextAppearanceSpan) textAppearanceSpan1;
        Assert.assertEquals(mGreenTextColor, textAppearanceSpan1Converted.getTextColor());

        // Test for red text color.
        MetricAffectingSpan textAppearanceSpan2 =
                AnswerTextNewLayout.getAppearanceForAnswerText(
                        mContext,
                        AnswerTextType.DESCRIPTION_POSITIVE,
                        AnswerType.ANSWER_TYPE_FINANCE,
                        true);
        verifyTextAppearanceSpan(textAppearanceSpan2);

        TextAppearanceSpan textAppearanceSpan2Converted = (TextAppearanceSpan) textAppearanceSpan2;
        Assert.assertEquals(mRedTextColor, textAppearanceSpan2Converted.getTextColor());
    }
}
