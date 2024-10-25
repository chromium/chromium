// Copyright 2022 The Chromium Authors
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

/** Tests for {@link CalculatorAnswerTextLayout}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CalculatorAnswerTextLayoutUnitTest {
    private Context mContext;
    private TextAppearanceSpan mPrimaryText;
    private TextAppearanceSpan mMediumText;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
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
    public void testCalculatorAnswerAppearance() {
        // Layout text appearance is different based on whether the instance holds the answer.
        CalculatorAnswerTextLayout answerLayout =
                new CalculatorAnswerTextLayout(mContext, "answer", /* isAnswer= */ true);
        CalculatorAnswerTextLayout displayTextLayout =
                new CalculatorAnswerTextLayout(mContext, "text", /* isAnswer= */ false);

        Assert.assertEquals(answerLayout.getText().toString(), "answer");
        Assert.assertEquals(displayTextLayout.getText().toString(), "text");

        SpannableStringBuilder answerLayoutText = answerLayout.getText();
        TextAppearanceSpan[] answerTextAppearanceSpans =
                answerLayoutText.getSpans(0, answerLayoutText.length(), TextAppearanceSpan.class);
        Assert.assertEquals(answerTextAppearanceSpans.length, 1);
        Assert.assertEquals(answerTextAppearanceSpans[0].getTextSize(), mPrimaryText.getTextSize());

        SpannableStringBuilder displayLayoutText = displayTextLayout.getText();
        TextAppearanceSpan[] displayTextAppearanceSpans =
                displayLayoutText.getSpans(0, displayLayoutText.length(), TextAppearanceSpan.class);
        Assert.assertEquals(displayTextAppearanceSpans.length, 1);
        Assert.assertEquals(displayTextAppearanceSpans[0].getTextSize(), mMediumText.getTextSize());
    }
}
