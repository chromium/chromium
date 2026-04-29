// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.answer;

import static org.junit.Assert.assertEquals;

import android.content.Context;
import android.text.SpannableStringBuilder;
import android.text.style.TextAppearanceSpan;
import android.view.ContextThemeWrapper;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.R;

/** Tests for {@link CalculatorAnswerTextLayout}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CalculatorAnswerTextLayoutUnitTest {
    private Context mContext;
    private TextAppearanceSpan mPrimaryText;
    private TextAppearanceSpan mMediumText;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        mPrimaryText = new TextAppearanceSpan(mContext, R.style.TextAppearance_TextLarge_Primary);
        mMediumText = new TextAppearanceSpan(mContext, R.style.TextAppearance_TextMedium_Secondary);
    }

    @Test
    @SmallTest
    public void testCalculatorAnswerAppearance() {
        // Layout text appearance is different based on whether the instance holds the answer.
        CalculatorAnswerTextLayout answerLayout =
                new CalculatorAnswerTextLayout(mContext, "answer", /* isAnswerLine= */ true);
        CalculatorAnswerTextLayout displayTextLayout =
                new CalculatorAnswerTextLayout(mContext, "text", /* isAnswerLine= */ false);

        assertEquals("answer", answerLayout.getText().toString());
        assertEquals("text", displayTextLayout.getText().toString());

        SpannableStringBuilder answerLayoutText = answerLayout.getText();
        TextAppearanceSpan[] answerTextAppearanceSpans =
                answerLayoutText.getSpans(0, answerLayoutText.length(), TextAppearanceSpan.class);
        assertEquals(1, answerTextAppearanceSpans.length);
        assertEquals(answerTextAppearanceSpans[0].getTextSize(), mPrimaryText.getTextSize());

        SpannableStringBuilder displayLayoutText = displayTextLayout.getText();
        TextAppearanceSpan[] displayTextAppearanceSpans =
                displayLayoutText.getSpans(0, displayLayoutText.length(), TextAppearanceSpan.class);
        assertEquals(1, displayTextAppearanceSpans.length);
        assertEquals(displayTextAppearanceSpans[0].getTextSize(), mMediumText.getTextSize());
    }
}
