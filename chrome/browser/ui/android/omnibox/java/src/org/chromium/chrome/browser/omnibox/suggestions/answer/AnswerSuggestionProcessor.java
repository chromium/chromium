// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.answer;

import android.content.Context;

import androidx.annotation.DrawableRes;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.LocaleUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.styles.OmniboxImageSupplier;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.base.SuggestionDrawableState;
import org.chromium.components.omnibox.AnswerType;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.SuggestionAnswer;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/**
 * A class that handles model and view creation for the most commonly used omnibox suggestion.
 */
public class AnswerSuggestionProcessor extends BaseSuggestionViewProcessor {
    private static final String COLOR_REVERSAL_COUNTRY_LIST = "ja-JP,ko-KR,zh-CN,zh-TW";

    private final UrlBarEditingTextStateProvider mUrlBarEditingTextProvider;
    private final @Nullable OmniboxImageSupplier mImageSupplier;
    private boolean mOmniBoxAnswerColorReversal;

    /**
     * @param context An Android context.
     * @param suggestionHost A handle to the object using the suggestions.
     */
    public AnswerSuggestionProcessor(Context context, SuggestionHost suggestionHost,
            UrlBarEditingTextStateProvider editingTextProvider,
            OmniboxImageSupplier imageSupplier) {
        super(context, suggestionHost, null);
        mUrlBarEditingTextProvider = editingTextProvider;
        mImageSupplier = imageSupplier;
    }

    /**
     * Evaluates whether the current locale uses "green" or "red" color to indicate
     * growth, allowing locale-adjusted representation of stock market changes.
     */
    @Override
    public void onNativeInitialized() {
        super.onNativeInitialized();
        mOmniBoxAnswerColorReversal =
                ChromeFeatureList.isEnabled(ChromeFeatureList.SUGGESTION_ANSWERS_COLOR_REVERSE);
    }

    @Override
    public boolean doesProcessSuggestion(AutocompleteMatch suggestion, int position) {
        // Calculation answers are specific in a way that these are basic suggestions, but processed
        // as answers, when new answer layout is enabled.
        return suggestion.hasAnswer() || suggestion.getType() == OmniboxSuggestionType.CALCULATOR;
    }

    @Override
    public int getViewTypeId() {
        return OmniboxSuggestionUiType.ANSWER_SUGGESTION;
    }

    @Override
    public PropertyModel createModel() {
        return new PropertyModel(AnswerSuggestionViewProperties.ALL_KEYS);
    }

    @Override
    public void populateModel(AutocompleteMatch suggestion, PropertyModel model, int position) {
        super.populateModel(suggestion, model, position);
        setStateForSuggestion(model, suggestion, position);
    }

    private void fetchAnswerImage(GURL imageUrl, PropertyModel model) {
        // Ensure an image fetcher is available prior to requesting images.
        if (mImageSupplier == null) return;
        mImageSupplier.fetchImage(imageUrl, bitmap -> {
            setSuggestionDrawableState(model,
                    SuggestionDrawableState.Builder.forBitmap(mContext, bitmap)
                            .setUseRoundedCorners(true)
                            .setLarge(true)
                            .build());
        });
    }

    /**
     * Sets both lines of the Omnibox suggestion based on an Answers in Suggest result.
     */
    private void setStateForSuggestion(
            PropertyModel model, AutocompleteMatch suggestion, int position) {
        @AnswerType
        int answerType = suggestion.getAnswer() == null ? AnswerType.INVALID
                                                        : suggestion.getAnswer().getType();
        boolean suggestionTextColorReversal = checkColorReversalRequired(answerType);
        AnswerText[] details = AnswerTextNewLayout.from(mContext, suggestion,
                mUrlBarEditingTextProvider.getTextWithoutAutocomplete(),
                suggestionTextColorReversal);

        model.set(AnswerSuggestionViewProperties.TEXT_LINE_1_TEXT, details[0].mText);
        model.set(AnswerSuggestionViewProperties.TEXT_LINE_2_TEXT, details[1].mText);

        model.set(AnswerSuggestionViewProperties.TEXT_LINE_1_ACCESSIBILITY_DESCRIPTION,
                details[0].mAccessibilityDescription);
        model.set(AnswerSuggestionViewProperties.TEXT_LINE_2_ACCESSIBILITY_DESCRIPTION,
                details[1].mAccessibilityDescription);

        model.set(AnswerSuggestionViewProperties.TEXT_LINE_1_MAX_LINES, details[0].mMaxLines);
        model.set(AnswerSuggestionViewProperties.TEXT_LINE_2_MAX_LINES, details[1].mMaxLines);

        setSuggestionDrawableState(model,
                SuggestionDrawableState.Builder
                        .forDrawableRes(mContext, getSuggestionIcon(suggestion))
                        .setLarge(true)
                        .build());

        setTabSwitchOrRefineAction(model, suggestion, position);
        if (suggestion.hasAnswer() && suggestion.getAnswer().getSecondLine().hasImage()) {
            fetchAnswerImage(new GURL(suggestion.getAnswer().getSecondLine().getImage()), model);
        }
    }

    /**
     * Checks if we need to apply color reversion on the answer suggestion.
     * @param answerType The type of a suggested answer.
     */
    @VisibleForTesting
    public boolean checkColorReversalRequired(@AnswerType int answerType) {
        boolean isFinanceAnswer = answerType == AnswerType.FINANCE;
        // Flag disabled.
        if (!mOmniBoxAnswerColorReversal) return false;
        // Country not eligible.
        if (!isCountryEligibleForColorReversal()) return false;
        // Not a finance answer.
        if (!isFinanceAnswer) return false;
        // All other cases.
        return true;
    }

    /**
     * Returns whether a given country is eligible for Answer color reversal.
     * Note: this call does not verify the flag state.
     */
    @VisibleForTesting
    /* package */ boolean isCountryEligibleForColorReversal() {
        return COLOR_REVERSAL_COUNTRY_LIST.contains(LocaleUtils.getDefaultLocaleString());
    }
    /**
     * Get default suggestion icon for supplied suggestion.
     */
    @DrawableRes
    int getSuggestionIcon(AutocompleteMatch suggestion) {
        SuggestionAnswer answer = suggestion.getAnswer();
        if (answer != null) {
            switch (answer.getType()) {
                case AnswerType.DICTIONARY:
                    return R.drawable.ic_book_round;
                case AnswerType.FINANCE:
                    return R.drawable.ic_swap_vert_round;
                case AnswerType.KNOWLEDGE_GRAPH:
                    return R.drawable.ic_google_round;
                case AnswerType.SUNRISE:
                    return R.drawable.ic_wb_sunny_round;
                case AnswerType.TRANSLATION:
                    return R.drawable.logo_translate_round;
                case AnswerType.WEATHER:
                    return R.drawable.logo_partly_cloudy;
                case AnswerType.WHEN_IS:
                    return R.drawable.ic_event_round;
                case AnswerType.CURRENCY:
                    return R.drawable.ic_loop_round;
                case AnswerType.SPORTS:
                    return R.drawable.ic_google_round;
                default:
                    break;
            }
        } else if (suggestion.getType() == OmniboxSuggestionType.CALCULATOR) {
            return R.drawable.ic_equals_sign_round;
        } else {
            assert false : "Requested Answer icon for non-answer suggestion";
        }
        return R.drawable.ic_google_round;
    }
}
