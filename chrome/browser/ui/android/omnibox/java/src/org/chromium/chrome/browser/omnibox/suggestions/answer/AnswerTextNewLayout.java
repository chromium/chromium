// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.answer;

import android.content.Context;
import android.text.Spannable;
import android.text.SpannableStringBuilder;
import android.text.style.MetricAffectingSpan;
import android.text.style.TextAppearanceSpan;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.omnibox.R;
import org.chromium.components.omnibox.AnswerTextType;
import org.chromium.components.omnibox.AnswerTypeProto.AnswerType;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.SuggestionAnswer;
import org.chromium.components.omnibox.SuggestionAnswer.TextField;

import java.util.List;

/**
 * AnswerTextNewLayout builds Omnibox styled Answer suggestion texts for revamped answer layouts.
 */
class AnswerTextNewLayout implements AnswerText {
    final Context mContext;
    private final boolean mIsAnswer;
    private final AnswerType mAnswerType;
    private final boolean mStockTextColorReverse;

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
     * Convert SuggestionAnswer to array of elements that directly translate to user-presented
     * content.
     *
     * @param context Current context.
     * @param suggestion Suggestion to be converted.
     * @param query Query that triggered the suggestion.
     * @param stockTextColorReverse flag to reverse the green-red color indicator on stock text.
     * @return array of AnswerText elements to use to construct suggestion item.
     */
    static AnswerText[] from(
            Context context,
            AutocompleteMatch suggestion,
            String query,
            boolean stockTextColorReverse) {
        AnswerTextNewLayout[] result = new AnswerTextNewLayout[2];

        SuggestionAnswer answer = suggestion.getAnswer();
        if (answer == null) {
            // As an exception, we handle calculation suggestions, too, considering them an Answer,
            // even if these are not one.
            assert suggestion.getType() == OmniboxSuggestionType.CALCULATOR;
            result[0] = new AnswerTextNewLayout(context, query, true);
            result[1] = new AnswerTextNewLayout(context, suggestion.getDisplayText(), false);
        } else if (answer.getType() == AnswerType.ANSWER_TYPE_DICTIONARY) {
            result[0] =
                    new AnswerTextNewLayout(
                            context,
                            answer.getType(),
                            answer.getFirstLine(),
                            true,
                            stockTextColorReverse);
            result[1] =
                    new AnswerTextNewLayout(
                            context,
                            answer.getType(),
                            answer.getSecondLine(),
                            false,
                            stockTextColorReverse);
            result[0].mMaxLines = 1;
        } else {
            // Construct the Answer card presenting AiS in Answer > Query order.
            // Note: Despite AiS being presented in reverse order (first answer, then query)
            // we want to ensure that the query is announced first to visually impaired people
            // to avoid confusion.
            result[0] =
                    new AnswerTextNewLayout(
                            context,
                            answer.getType(),
                            answer.getSecondLine(),
                            true,
                            stockTextColorReverse);
            result[1] =
                    new AnswerTextNewLayout(
                            context,
                            answer.getType(),
                            answer.getFirstLine(),
                            false,
                            stockTextColorReverse);
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
    AnswerTextNewLayout(
            Context context,
            AnswerType type,
            SuggestionAnswer.ImageLine line,
            boolean isAnswerLine,
            boolean stockTextColorReverse) {
        mContext = context;
        mIsAnswer = isAnswerLine;
        mAnswerType = type;
        mStockTextColorReverse = stockTextColorReverse;
        build(line);
    }

    /**
     * Create new instance of AnswerTextNewLayout for non-answer suggestions.
     *
     * @param context Current context.
     * @param text Suggestion text.
     * @param isAnswerLine True, if this instance holds answer.
     */
    AnswerTextNewLayout(Context context, String text, boolean isAnswerLine) {
        mContext = context;
        mIsAnswer = isAnswerLine;
        mAnswerType = AnswerType.ANSWER_TYPE_UNSPECIFIED;
        mStockTextColorReverse = false;
        appendAndStyleText(text, getAppearanceForText(AnswerTextType.SUGGESTION));
    }

    /**
     * Builds a Spannable containing all of the styled text in the supplied ImageLine.
     *
     * @param line All text fields within this line will be used to build the resulting content.
     */
    private void build(SuggestionAnswer.ImageLine line) {
        List<TextField> textFields = line.getTextFields();
        for (int i = 0; i < textFields.size(); i++) {
            appendAndStyleText(
                    textFields.get(i).getText(), getAppearanceForText(textFields.get(i).getType()));
            if (textFields.get(i).hasNumLines()) {
                mMaxLines = Math.max(mMaxLines, Math.min(3, textFields.get(i).getNumLines()));
            }
        }

        if (line.hasAdditionalText()) {
            mText.append("  ");
            appendAndStyleText(
                    line.getAdditionalText().getText(),
                    getAppearanceForText(line.getAdditionalText().getType()));
        }
        if (line.hasStatusText()) {
            mText.append("  ");
            appendAndStyleText(
                    line.getStatusText().getText(),
                    getAppearanceForText(line.getStatusText().getType()));
        }

        mAccessibilityDescription = mText.toString();
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
     * @param type The answer type as specified at <a href="http://goto.google.com/ais_api">...</a>.
     * @return TextAppearanceSpan specifying style to be used to present text field.
     */
    private MetricAffectingSpan getAppearanceForText(@AnswerTextType int type) {
        return mIsAnswer
                ? getAppearanceForAnswerText(mContext, type, mAnswerType, mStockTextColorReverse)
                : getAppearanceForQueryText();
    }

    /**
     * Return text style for elements in main line holding answer.
     *
     * @param type The answer text type for the suggestion answer.
     * @param answerType the answer type for the suggestion answer
     * @param context Current context.
     * @param stockTextColorReverse flag to indicate whether we need to reverse the text color to
     *     match positive/negative color meanings in certain countries.
     * @return TextAppearanceSpan object defining style for the text.
     */
    @VisibleForTesting
    static MetricAffectingSpan getAppearanceForAnswerText(
            Context context,
            @AnswerTextType int type,
            AnswerType answerType,
            boolean stockTextColorReverse) {
        if (answerType != AnswerType.ANSWER_TYPE_DICTIONARY
                && answerType != AnswerType.ANSWER_TYPE_FINANCE) {
            return new TextAppearanceSpan(context, R.style.TextAppearance_TextLarge_Primary);
        }

        switch (type) {
            case AnswerTextType.DESCRIPTION_NEGATIVE -> {
                return new TextAppearanceSpan(
                        context,
                        stockTextColorReverse
                                ? R.style.TextAppearance_OmniboxAnswerDescriptionPositiveSmall
                                : R.style.TextAppearance_OmniboxAnswerDescriptionNegativeSmall);
            }
            case AnswerTextType.DESCRIPTION_POSITIVE -> {
                return new TextAppearanceSpan(
                        context,
                        stockTextColorReverse
                                ? R.style.TextAppearance_OmniboxAnswerDescriptionNegativeSmall
                                : R.style.TextAppearance_OmniboxAnswerDescriptionPositiveSmall);
            }
            case AnswerTextType.SUGGESTION_SECONDARY_TEXT_MEDIUM -> {
                return new TextAppearanceSpan(context, R.style.TextAppearance_TextSmall_Secondary);
            }
            case AnswerTextType.SUGGESTION,
                    AnswerTextType.PERSONALIZED_SUGGESTION,
                    AnswerTextType.ANSWER_TEXT_MEDIUM,
                    AnswerTextType.ANSWER_TEXT_LARGE,
                    AnswerTextType.TOP_ALIGNED,
                    AnswerTextType.SUGGESTION_SECONDARY_TEXT_SMALL -> {
                return new TextAppearanceSpan(context, R.style.TextAppearance_TextLarge_Primary);
            }
            default -> {
                assert false : "Unknown answer type: " + type;
                return new TextAppearanceSpan(context, R.style.TextAppearance_TextLarge_Primary);
            }
        }
    }

    /**
     * Return text style for elements in second line holding query.
     *
     * @return TextAppearanceSpan object defining style for the text.
     */
    private MetricAffectingSpan getAppearanceForQueryText() {
        return new TextAppearanceSpan(mContext, R.style.TextAppearance_TextMedium_Secondary);
    }
}
