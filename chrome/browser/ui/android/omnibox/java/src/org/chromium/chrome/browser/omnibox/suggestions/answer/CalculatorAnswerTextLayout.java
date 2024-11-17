// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.answer;

import android.content.Context;
import android.text.Spannable;
import android.text.SpannableStringBuilder;
import android.text.style.MetricAffectingSpan;
import android.text.style.TextAppearanceSpan;

import org.chromium.chrome.browser.omnibox.R;
import org.chromium.components.omnibox.AnswerTypeProto.AnswerType;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.OmniboxSuggestionType;

/**
 * CalculatorAnswerTextLayout builds Omnibox styled calculator answer suggestion texts for revamped
 * answer layouts.
 */
class CalculatorAnswerTextLayout implements AnswerText {
    final Context mContext;
    private final boolean mIsAnswer;
    private final AnswerType mAnswerType;

    /** Content of the line of text in omnibox suggestion. */
    private final SpannableStringBuilder mText = new SpannableStringBuilder();

    private String mAccessibilityDescription;
    private int mMaxLines = 1;

    // AnswerText implementation.
    @Override
    public SpannableStringBuilder getText() {
        return mText;
    }

    @Override
    public String getAccessibilityDescription() {
        return mAccessibilityDescription;
    }

    @Override
    public int getMaxLines() {
        return mMaxLines;
    }

    /**
     * Convert calculator answer to array of elements that directly translate to user-presented
     * content.
     *
     * @param context Current context.
     * @param suggestion Suggestion to be converted.
     * @param query Query that triggered the suggestion.
     * @return array of AnswerText elements to use to construct suggestion item.
     */
    static AnswerText[] from(Context context, AutocompleteMatch suggestion, String query) {
        assert suggestion.getType() == OmniboxSuggestionType.CALCULATOR;
        CalculatorAnswerTextLayout[] result = new CalculatorAnswerTextLayout[2];
        result[0] = new CalculatorAnswerTextLayout(context, query, true);
        result[1] = new CalculatorAnswerTextLayout(context, suggestion.getDisplayText(), false);
        return result;
    }

    /**
     * Create new instance of CalculatorAnswerTextLayout for calculator answers.
     *
     * @param context Current context.
     * @param text Suggestion text.
     * @param isAnswerLine True, if this instance holds answer.
     */
    CalculatorAnswerTextLayout(Context context, String text, boolean isAnswerLine) {
        mContext = context;
        mIsAnswer = isAnswerLine;
        mAnswerType = AnswerType.ANSWER_TYPE_UNSPECIFIED;
        appendAndStyleText(text, getAppearanceForText());
    }

    private void appendAndStyleText(String text, MetricAffectingSpan style) {
        // Unescape HTML entities (e.g. "&quot;", "&gt;").
        text = AnswerTextUtils.processAnswerText(text, mIsAnswer, mAnswerType);

        // Append as HTML (answer responses contain simple markup).
        int start = mText.length();
        mText.append(text);
        int end = mText.length();
        mText.setSpan(style, start, end, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
    }

    /**
     * Return the TextAppearanceSpan specifying text decorations for a given field type.
     *
     * @return TextAppearanceSpan specifying style to be used to present text field.
     */
    private MetricAffectingSpan getAppearanceForText() {
        return mIsAnswer
                ? new TextAppearanceSpan(mContext, R.style.TextAppearance_TextLarge_Primary)
                : new TextAppearanceSpan(mContext, R.style.TextAppearance_TextMedium_Secondary);
    }
}
