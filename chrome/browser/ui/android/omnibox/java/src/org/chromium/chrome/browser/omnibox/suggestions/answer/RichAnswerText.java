// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.answer;

import android.content.Context;
import android.text.Spannable;
import android.text.SpannableStringBuilder;
import android.text.style.MetricAffectingSpan;
import android.text.style.TextAppearanceSpan;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.components.omnibox.AnswerDataProto.FormattedString;
import org.chromium.components.omnibox.AnswerDataProto.FormattedString.ColorType;
import org.chromium.components.omnibox.AnswerDataProto.FormattedString.FormattedStringFragment;
import org.chromium.components.omnibox.AnswerType;
import org.chromium.components.omnibox.RichAnswerTemplateProto.RichAnswerTemplate;

import java.util.List;

/**
 * {@link AnswerText} implementation based on RichAnswerTemplate as the source of answer lines (as
 * opposed to SuggestionAnswer, implemented by {@link AnswerTextNewLayout}).
 */
class RichAnswerText implements AnswerText {

    /** Content of the line of text in omnibox suggestion. */
    private final SpannableStringBuilder mText;

    private final Context mContext;
    private final boolean mIsAnswerLine;
    private final boolean mReverseStockTextColor;
    private String mAccessibilityDescription;
    private int mMaxLines = 1;
    @AnswerType private final int mAnswerType;

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

    /** Construct an array of two AnswerText instances for the given RichAnswerTemplate. */
    static AnswerText[] from(
            @NonNull Context context,
            @NonNull RichAnswerTemplate richAnswerTemplate,
            boolean reverseStockTextColor) {
        RichAnswerText[] result = new RichAnswerText[2];

        int answerType = richAnswerTemplate.getAnswerType().getNumber();
        if (answerType == AnswerType.DICTIONARY) {
            result[0] =
                    new RichAnswerText(
                            context,
                            richAnswerTemplate.getAnswers(0).getHeadline(),
                            answerType,
                            /* isAnswerLine= */ true,
                            reverseStockTextColor);
            result[1] =
                    new RichAnswerText(
                            context,
                            richAnswerTemplate.getAnswers(0).getSubhead(),
                            answerType,
                            /* isAnswerLine= */ false,
                            reverseStockTextColor);
            result[0].mMaxLines = 1;
        } else {
            // Construct the Answer card presenting Answers in Suggest in Answer > Query order.
            result[0] =
                    new RichAnswerText(
                            context,
                            richAnswerTemplate.getAnswers(0).getSubhead(),
                            answerType,
                            /* isAnswerLine= */ true,
                            reverseStockTextColor);
            result[1] =
                    new RichAnswerText(
                            context,
                            richAnswerTemplate.getAnswers(0).getHeadline(),
                            answerType,
                            /* isAnswerLine= */ false,
                            reverseStockTextColor);
            result[1].mMaxLines = 1;
            if (answerType == AnswerType.TRANSLATION) {
                result[0].mMaxLines = 3;
            }

            // Note: Despite Answers in Suggest being presented in reverse order (first answer, then
            // query) we want to ensure that the query is announced first to visually impaired
            // people to avoid confusion, so we swap a11y texts.
            String temp = result[1].mAccessibilityDescription;
            result[1].mAccessibilityDescription = result[0].mAccessibilityDescription;
            result[0].mAccessibilityDescription = temp;
        }
        return result;
    }

    private RichAnswerText(
            Context context,
            FormattedString formattedString,
            int answerType,
            boolean isAnswerLine,
            boolean reverseStockTextColor) {
        mContext = context;
        mAnswerType = answerType;
        mIsAnswerLine = isAnswerLine;
        mReverseStockTextColor = reverseStockTextColor;
        mText = processFormattedString(formattedString);
        mAccessibilityDescription = mText.toString();
    }

    private SpannableStringBuilder processFormattedString(FormattedString formattedString) {
        SpannableStringBuilder result = new SpannableStringBuilder();
        List<FormattedStringFragment> fragments = formattedString.getFragmentsList();
        // TODO(b/327497146): handle the case where there are no fragments by using the default for
        // the type of line.
        for (int i = 0; i < fragments.size(); i++) {
            FormattedStringFragment formattedStringFragment = fragments.get(i);
            appendAndStyleText(
                    formattedStringFragment, getAppearanceForText(formattedStringFragment), result);
        }

        return result;
    }

    private void appendAndStyleText(
            FormattedStringFragment formattedStringFragment,
            MetricAffectingSpan style,
            SpannableStringBuilder result) {
        if (!result.toString().isEmpty()) {
            result.append(" ");
        }

        String text =
                AnswerTextUtils.processAnswerText(
                        formattedStringFragment.getText(), mIsAnswerLine, mAnswerType);
        int startIndex = result.length();
        result.append(text);
        result.setSpan(style, startIndex, result.length(), Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
    }

    /**
     * Return the TextAppearanceSpan specifying text decorations for a given fragment.
     *
     * @return MetricAffectingSpan specifying style to be used to present text field.
     */
    private MetricAffectingSpan getAppearanceForText(
            FormattedStringFragment formattedStringFragment) {
        return mIsAnswerLine
                ? getAppearanceForAnswerText(
                        mContext, formattedStringFragment, mAnswerType, mReverseStockTextColor)
                : getAppearanceForQueryText();
    }

    /**
     * Return text style for elements in main line holding answer.
     *
     * @param answerType the answer type for the suggestion answer
     * @param context Current context.
     * @return TextAppearanceSpan object defining style for the text.
     */
    @VisibleForTesting
    static MetricAffectingSpan getAppearanceForAnswerText(
            Context context,
            FormattedStringFragment formattedStringFragment,
            @AnswerType int answerType,
            boolean reverseStockTextColor) {
        if (answerType != AnswerType.DICTIONARY && answerType != AnswerType.FINANCE) {
            return new TextAppearanceSpan(
                    context,
                    org.chromium.chrome.browser.omnibox.R.style.TextAppearance_TextLarge_Primary);
        }

        // TODO(b/327497146): skip color reversal when original data source is proto backend, which
        // should handle color reversal server side.
        ColorType colorType = formattedStringFragment.getColor();
        return switch (colorType) {
            case COLOR_ON_SURFACE_POSITIVE, COLOR_ON_SURFACE_NEGATIVE -> {
                boolean wantPositiveColor = (colorType == ColorType.COLOR_ON_SURFACE_POSITIVE);
                // Swap positive/negative colors if applicable for current locale.
                wantPositiveColor ^= reverseStockTextColor;
                int styleResource =
                        wantPositiveColor
                                ? org.chromium.chrome.browser.omnibox.R.style
                                        .TextAppearance_OmniboxAnswerDescriptionPositiveSmall
                                : org.chromium.chrome.browser.omnibox.R.style
                                        .TextAppearance_OmniboxAnswerDescriptionNegativeSmall;
                yield new TextAppearanceSpan(context, styleResource);
            }
            default -> new TextAppearanceSpan(
                    context,
                    org.chromium.chrome.browser.omnibox.R.style.TextAppearance_TextLarge_Primary);
                // TODO(b/327497146): handle equivalent of
                // AnswerTextType.SUGGESTION_SECONDARY_TEXT_MEDIUM
        };
    }

    /**
     * Return text style for elements in second line holding query.
     *
     * @return MetricAffectingSpan object defining style for the text.
     */
    private MetricAffectingSpan getAppearanceForQueryText() {
        return new TextAppearanceSpan(
                mContext,
                org.chromium.chrome.browser.omnibox.R.style.TextAppearance_TextMedium_Secondary);
    }
}
