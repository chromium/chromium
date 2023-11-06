// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.answer;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.LocaleUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.styles.OmniboxDrawableState;
import org.chromium.chrome.browser.omnibox.styles.OmniboxImageSupplier;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProcessor;
import org.chromium.components.omnibox.AnswerType;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.SuggestionAnswer;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/** A class that handles model and view creation for the most commonly used omnibox suggestion. */
public class AnswerSuggestionProcessor extends BaseSuggestionViewProcessor {
    private static final String COLOR_REVERSAL_COUNTRY_LIST = "ja-JP,ko-KR,zh-CN,zh-TW";

    private final UrlBarEditingTextStateProvider mUrlBarEditingTextProvider;
    private boolean mOmniBoxAnswerColorReversal;

    public AnswerSuggestionProcessor(
            Context context,
            SuggestionHost suggestionHost,
            UrlBarEditingTextStateProvider editingTextProvider,
            OmniboxImageSupplier imageSupplier) {
        super(context, suggestionHost, imageSupplier);
        mUrlBarEditingTextProvider = editingTextProvider;
    }

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

    private void setStateForSuggestion(
            PropertyModel model, AutocompleteMatch suggestion, int position) {
        @AnswerType
        int answerType =
                suggestion.getAnswer() == null
                        ? AnswerType.INVALID
                        : suggestion.getAnswer().getType();
        boolean suggestionTextColorReversal = checkColorReversalRequired(answerType);
        AnswerText[] details =
                AnswerTextNewLayout.from(
                        mContext,
                        suggestion,
                        mUrlBarEditingTextProvider.getTextWithoutAutocomplete(),
                        suggestionTextColorReversal);

        model.set(AnswerSuggestionViewProperties.TEXT_LINE_1_TEXT, details[0].mText);
        model.set(AnswerSuggestionViewProperties.TEXT_LINE_2_TEXT, details[1].mText);

        model.set(
                AnswerSuggestionViewProperties.TEXT_LINE_1_ACCESSIBILITY_DESCRIPTION,
                details[0].mAccessibilityDescription);
        model.set(
                AnswerSuggestionViewProperties.TEXT_LINE_2_ACCESSIBILITY_DESCRIPTION,
                details[1].mAccessibilityDescription);

        model.set(AnswerSuggestionViewProperties.TEXT_LINE_1_MAX_LINES, details[0].mMaxLines);
        model.set(AnswerSuggestionViewProperties.TEXT_LINE_2_MAX_LINES, details[1].mMaxLines);

        setTabSwitchOrRefineAction(model, suggestion, position);
        if (suggestion.hasAnswer() && suggestion.getAnswer().getSecondLine().hasImage()) {
            fetchImage(model, new GURL(suggestion.getAnswer().getSecondLine().getImage()));
        }
    }

    /**
     * Checks if we need to apply color reversion on the answer suggestion.
     *
     * @param answerType The type of a suggested answer.
     * @return true, if red/green colors should be swapped.
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

    /** Returns whether current Locale country is eligible for Answer color reversal. */
    @VisibleForTesting
    /* package */ boolean isCountryEligibleForColorReversal() {
        return COLOR_REVERSAL_COUNTRY_LIST.contains(LocaleUtils.getDefaultLocaleString());
    }

    @Override
    public OmniboxDrawableState getFallbackIcon(AutocompleteMatch suggestion) {
        int icon = 0;

        SuggestionAnswer answer = suggestion.getAnswer();
        if (answer != null) {
            switch (answer.getType()) {
                case AnswerType.DICTIONARY:
                    icon = R.drawable.ic_book_round;
                    break;
                case AnswerType.FINANCE:
                    icon = R.drawable.ic_swap_vert_round;
                    break;
                case AnswerType.KNOWLEDGE_GRAPH:
                    icon = R.drawable.ic_google_round;
                    break;
                case AnswerType.SUNRISE:
                    icon = R.drawable.ic_wb_sunny_round;
                    break;
                case AnswerType.TRANSLATION:
                    icon = R.drawable.logo_translate_round;
                    break;
                case AnswerType.WEATHER:
                    icon = R.drawable.logo_partly_cloudy;
                    break;
                case AnswerType.WHEN_IS:
                    icon = R.drawable.ic_event_round;
                    break;
                case AnswerType.CURRENCY:
                    icon = R.drawable.ic_loop_round;
                    break;
                case AnswerType.SPORTS:
                    icon = R.drawable.ic_google_round;
                    break;
            }
        } else if (suggestion.getType() == OmniboxSuggestionType.CALCULATOR) {
            icon = R.drawable.ic_equals_sign_round;
        }

        return icon == 0
                ? super.getFallbackIcon(suggestion)
                : OmniboxDrawableState.forLargeIcon(mContext, icon, /* allowTint= */ false);
    }
}
