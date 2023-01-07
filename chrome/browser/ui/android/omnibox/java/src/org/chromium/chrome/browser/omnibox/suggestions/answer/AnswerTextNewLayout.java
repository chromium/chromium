// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.answer;

import android.content.Context;
import android.text.style.MetricAffectingSpan;
import android.text.style.TextAppearanceSpan;

import androidx.annotation.StyleRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.components.omnibox.AnswerTextType;
import org.chromium.components.omnibox.AnswerType;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.SuggestionAnswer;

/**
 * AnswerTextNewLayout builds Omnibox styled Answer suggestion texts for revamped answer layouts.
 */
class AnswerTextNewLayout extends AnswerText {
    private static final String TAG = "AnswerTextNewLayout";
    private final boolean mIsAnswer;
    private final @AnswerType int mAnswerType;
    private final boolean mStockTextColorReverse;

    /**
     * Convert SuggestionAnswer to array of elements that directly translate to user-presented
     * content.
     *
     * @param context Current context.
     * @param suggestion Suggestion to be converted.
     * @param query Query that triggered the suggestion.
     * @param stockTextColorReverse flag to reverse the green-red color indicator on stock text.
     * @return array of AnswerText elements to use to construct suggestion item.
     */
    static AnswerText[] from(Context context, AutocompleteMatch suggestion, String query,
            boolean stockTextColorReverse) {
        AnswerText[] result = new AnswerText[2];

        SuggestionAnswer answer = suggestion.getAnswer();
        if (answer == null) {
            // As an exception, we handle calculation suggestions, too, considering them an Answer,
            // even if these are not one.
            assert suggestion.getType() == OmniboxSuggestionType.CALCULATOR;
            result[0] = new AnswerTextNewLayout(context, query, true);
            result[1] = new AnswerTextNewLayout(context, suggestion.getDisplayText(), false);
        } else if (answer.getType() == AnswerType.DICTIONARY) {
            result[0] = new AnswerTextNewLayout(
                    context, answer.getType(), answer.getFirstLine(), true, stockTextColorReverse);
            result[1] = new AnswerTextNewLayout(context, answer.getType(), answer.getSecondLine(),
                    false, stockTextColorReverse);
            result[0].mMaxLines = 1;
        } else {
            // Construct the Answer card presenting AiS in Answer > Query order.
            // Note: Despite AiS being presented in reverse order (first answer, then query)
            // we want to ensure that the query is announced first to visually impaired people
            // to avoid confusion.
            result[0] = new AnswerTextNewLayout(
                    context, answer.getType(), answer.getSecondLine(), true, stockTextColorReverse);
            result[1] = new AnswerTextNewLayout(
                    context, answer.getType(), answer.getFirstLine(), false, stockTextColorReverse);
            result[1].mMaxLines = 1;

            String temp = result[1].mAccessibilityDescription;
            result[1].mAccessibilityDescription = result[0].mAccessibilityDescription;
            result[0].mAccessibilityDescription = temp;
        }

        return result;
    }

    /**
     * Create new instance of AnswerTextNewLayout for answer suggestions.
     *
     * @param context Current context.
     * @param type Answer type, eg. AnswerType.WEATHER.
     * @param line Suggestion line that will be converted to Answer Text.
     * @param isAnswerLine True, if this instance holds answer.
     */
    AnswerTextNewLayout(Context context, @AnswerType int type, SuggestionAnswer.ImageLine line,
            boolean isAnswerLine, boolean stockTextColorReverse) {
        super(context);
        mIsAnswer = isAnswerLine;
        mAnswerType = type;
        mStockTextColorReverse = stockTextColorReverse;
        build(line);
    }

    /**
     * Create new instance of AnswerTextNewLayout for non-answer suggestions.
     * @param context Current context.
     * @param text Suggestion text.
     * @param isAnswerLine True, if this instance holds answer.
     */
    AnswerTextNewLayout(Context context, String text, boolean isAnswerLine) {
        super(context);
        mIsAnswer = isAnswerLine;
        mAnswerType = AnswerType.INVALID;
        mStockTextColorReverse = false;
        appendAndStyleText(text, getAppearanceForText(AnswerTextType.SUGGESTION));
    }

    /**
     * Process (if desired) content of the answer text.
     *
     * @param text Source text.
     * @return Either original or modified text.
     */
    @Override
    protected String processAnswerText(String text) {
        if (mIsAnswer && mAnswerType == AnswerType.CURRENCY) {
            // Modify the content of answer to present only the value after conversion, that is:
            //    1,000 United State Dollar = 1,330.75 Canadian Dollar
            // becomes
            //    1,330.75 Canadian Dollar
            int offset = text.indexOf(" = ");
            if (offset > 0) {
                text = text.substring(offset + 3);
            }
        }
        return text;
    }

    /**
     * Return the TextAppearanceSpan array specifying text decorations for a given field type.
     *
     * @param type The answer type as specified at http://goto.google.com/ais_api.
     * @return TextAppearanceSpan array specifying styles to be used to present text field.
     */
    @Override
    protected MetricAffectingSpan[] getAppearanceForText(@AnswerTextType int type) {
        return mIsAnswer
                ? getAppearanceForAnswerText(mContext, type, mAnswerType, mStockTextColorReverse)
                : getAppearanceForQueryText(type);
    }

    /**
     * Return text styles for elements in main line holding answer.
     *
     * @param type The answer text type for the suggestion answer.
     * @param answerType the answer type for the suggestion answer
     * @param context Current context.
     * @param stockTextColorReverse flag to indicate whether we need to reverse the text color to
     *         match positive/negative color meanings in certain countries.
     * @return array of TextAppearanceSpan objects defining style for the text.
     */
    @VisibleForTesting
    static MetricAffectingSpan[] getAppearanceForAnswerText(Context context,
            @AnswerTextType int type, @AnswerType int answerType, boolean stockTextColorReverse) {
        if (answerType != AnswerType.DICTIONARY && answerType != AnswerType.FINANCE) {
            return new TextAppearanceSpan[] {
                    new TextAppearanceSpan(context, R.style.TextAppearance_TextLarge_Primary)};
        }

        @StyleRes
        int res = 0;
        switch (type) {
            case AnswerTextType.DESCRIPTION_NEGATIVE:
                res = stockTextColorReverse
                        ? R.style.TextAppearance_OmniboxAnswerDescriptionPositiveSmall
                        : R.style.TextAppearance_OmniboxAnswerDescriptionNegativeSmall;
                break;

            case AnswerTextType.DESCRIPTION_POSITIVE:
                res = stockTextColorReverse
                        ? R.style.TextAppearance_OmniboxAnswerDescriptionNegativeSmall
                        : R.style.TextAppearance_OmniboxAnswerDescriptionPositiveSmall;
                break;

            case AnswerTextType.SUGGESTION_SECONDARY_TEXT_MEDIUM:
                res = R.style.TextAppearance_TextSmall_Secondary;
                break;

            case AnswerTextType.SUGGESTION:
            case AnswerTextType.PERSONALIZED_SUGGESTION:
            case AnswerTextType.ANSWER_TEXT_MEDIUM:
            case AnswerTextType.ANSWER_TEXT_LARGE:
            case AnswerTextType.TOP_ALIGNED:
            case AnswerTextType.SUGGESTION_SECONDARY_TEXT_SMALL:
                res = R.style.TextAppearance_TextLarge_Primary;
                break;

            default:
                assert false : "Unknown answer type: " + type;
                res = R.style.TextAppearance_TextLarge_Primary;
                break;
        }

        return new TextAppearanceSpan[] {new TextAppearanceSpan(context, res)};
    }

    /**
     * Return text styles for elements in second line holding query.
     *
     * @param type The answer type as specified at http://goto.google.com/ais_api.
     * @return array of TextAppearanceSpan objects defining style for the text.
     */
    private MetricAffectingSpan[] getAppearanceForQueryText(@AnswerTextType int type) {
        return new TextAppearanceSpan[] {
                new TextAppearanceSpan(mContext, R.style.TextAppearance_TextMedium_Secondary)};
    }
}
