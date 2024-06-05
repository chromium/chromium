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
import androidx.annotation.StyleRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.components.omnibox.AnswerDataProto.FormattedString;
import org.chromium.components.omnibox.AnswerDataProto.FormattedString.ColorType;
import org.chromium.components.omnibox.AnswerDataProto.FormattedString.FormattedStringFragment;
import org.chromium.components.omnibox.AnswerType;
import org.chromium.components.omnibox.OmniboxFeatures;
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
        int maxLines = getMaxLinesForAnswerType(answerType);
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
            result[1].mMaxLines = maxLines;
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
            result[0].mMaxLines = maxLines;

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
        if (fragments.size() > 0) {
            for (int i = 0; i < fragments.size(); i++) {
                FormattedStringFragment formattedStringFragment = fragments.get(i);
                String text =
                        AnswerTextUtils.processAnswerText(
                                formattedStringFragment.getText(), mIsAnswerLine, mAnswerType);
                appendAndStyleText(
                        text, getAppearanceForText(formattedStringFragment.getColor()), result);
            }
        } else {
            String text =
                    AnswerTextUtils.processAnswerText(
                            formattedString.getText(), mIsAnswerLine, mAnswerType);
            appendAndStyleText(
                    text, getAppearanceForText(ColorType.COLOR_ON_SURFACE_DEFAULT), result);
        }

        return result;
    }

    private void appendAndStyleText(
            String text, MetricAffectingSpan style, SpannableStringBuilder result) {
        if (!result.toString().isEmpty()) {
            result.append(" ");
        }

        int startIndex = result.length();
        result.append(text);
        result.setSpan(style, startIndex, result.length(), Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
    }

    /**
     * Return the TextAppearanceSpan specifying text decorations for a given fragment.
     *
     * @return MetricAffectingSpan specifying style to be used to present text field.
     */
    private MetricAffectingSpan getAppearanceForText(ColorType colorType) {
        return mIsAnswerLine
                ? getAppearanceForAnswerText(
                        mContext, colorType, mAnswerType, mReverseStockTextColor)
                : getAppearanceForQueryText();
    }

    /**
     * Return text style for elements in main line holding answer.
     *
     * @param colorType The color type for the text.
     * @param context Current context.
     * @return TextAppearanceSpan object defining style for the text.
     */
    @VisibleForTesting
    static MetricAffectingSpan getAppearanceForAnswerText(
            Context context,
            ColorType colorType,
            @AnswerType int answerType,
            boolean reverseStockTextColor) {
        @StyleRes
        int largeRes =
                OmniboxFeatures.shouldShowRichAnswerCard()
                        ? org.chromium.chrome.browser.omnibox.R.style
                                .TextAppearance_OmniboxAnswerCardPrimaryMedium
                        : org.chromium.chrome.browser.omnibox.R.style
                                .TextAppearance_TextLarge_Primary;
        if (answerType != AnswerType.DICTIONARY && answerType != AnswerType.FINANCE) {
            return new TextAppearanceSpan(context, largeRes);
        }

        // TODO(b/327497146): skip color reversal when original data source is proto backend, which
        // should handle color reversal server side.
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
            default -> new TextAppearanceSpan(context, largeRes);
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
        @StyleRes
        int res =
                OmniboxFeatures.shouldShowRichAnswerCard()
                        ? org.chromium.chrome.browser.omnibox.R.style
                                .TextAppearance_TextLarge_Secondary
                        : org.chromium.chrome.browser.omnibox.R.style
                                .TextAppearance_TextMedium_Secondary;
        return new TextAppearanceSpan(mContext, res);
    }

    private static int getMaxLinesForAnswerType(int answerType) {
        return (answerType == AnswerType.DICTIONARY || answerType == AnswerType.TRANSLATION)
                ? 3
                : 1;
    }
}
