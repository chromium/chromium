// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.answer;

import static org.hamcrest.core.IsInstanceOf.instanceOf;

import android.content.Context;
import android.content.res.ColorStateList;
import android.text.style.MetricAffectingSpan;
import android.text.style.TextAppearanceSpan;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.components.omnibox.AnswerTextType;
import org.chromium.components.omnibox.AnswerType;

/**
 * Tests for {@link AnswerTextNewLayout}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class AnswerTextNewLayoutUnitTest {
    private Context mContext;
    private ColorStateList mGreenTextColor;
    private ColorStateList mRedTextColor;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
        mGreenTextColor = new TextAppearanceSpan(
                mContext, R.style.TextAppearance_OmniboxAnswerDescriptionPositiveSmall)
                                  .getTextColor();
        mRedTextColor = new TextAppearanceSpan(
                mContext, R.style.TextAppearance_OmniboxAnswerDescriptionNegativeSmall)
                                .getTextColor();
    }

    /** Check the validity of TextAppearanceSpan. */
    private void verifyTextAppearanceSpan(MetricAffectingSpan[] textAppearanceSpan) {
        Assert.assertEquals(1, textAppearanceSpan.length);
        Assert.assertThat(textAppearanceSpan[0], instanceOf(TextAppearanceSpan.class));
    }

    /**
     * GetAppearanceForAnswerText should produce the correct color for stock ticker when the flag
     * for reversing Stock Ticker Color is disabled.
     */
    @Test
    @SmallTest
    public void getAppearanceForAnswerText_noColorReversal() {
        // Test for red text color.
        MetricAffectingSpan[] textAppearanceSpan1 = AnswerTextNewLayout.getAppearanceForAnswerText(
                mContext, AnswerTextType.DESCRIPTION_NEGATIVE, AnswerType.FINANCE, false);
        verifyTextAppearanceSpan(textAppearanceSpan1);

        TextAppearanceSpan textAppearanceSpan1Converted =
                (TextAppearanceSpan) textAppearanceSpan1[0];
        Assert.assertEquals(mRedTextColor, textAppearanceSpan1Converted.getTextColor());

        // Test for green text color.
        MetricAffectingSpan[] textAppearanceSpan2 = AnswerTextNewLayout.getAppearanceForAnswerText(
                mContext, AnswerTextType.DESCRIPTION_POSITIVE, AnswerType.FINANCE, false);
        verifyTextAppearanceSpan(textAppearanceSpan2);

        TextAppearanceSpan textAppearanceSpan2Converted =
                (TextAppearanceSpan) textAppearanceSpan2[0];
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
        MetricAffectingSpan[] textAppearanceSpan1 = AnswerTextNewLayout.getAppearanceForAnswerText(
                mContext, AnswerTextType.DESCRIPTION_NEGATIVE, AnswerType.FINANCE, true);
        verifyTextAppearanceSpan(textAppearanceSpan1);

        TextAppearanceSpan textAppearanceSpan1Converted =
                (TextAppearanceSpan) textAppearanceSpan1[0];
        Assert.assertEquals(mGreenTextColor, textAppearanceSpan1Converted.getTextColor());

        // Test for red text color.
        MetricAffectingSpan[] textAppearanceSpan2 = AnswerTextNewLayout.getAppearanceForAnswerText(
                mContext, AnswerTextType.DESCRIPTION_POSITIVE, AnswerType.FINANCE, true);
        verifyTextAppearanceSpan(textAppearanceSpan2);

        TextAppearanceSpan textAppearanceSpan2Converted =
                (TextAppearanceSpan) textAppearanceSpan2[0];
        Assert.assertEquals(mRedTextColor, textAppearanceSpan2Converted.getTextColor());
    }
}