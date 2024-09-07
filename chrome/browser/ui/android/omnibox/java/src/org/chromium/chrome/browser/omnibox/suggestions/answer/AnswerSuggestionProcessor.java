// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.answer;

import android.content.Context;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.LocaleUtils;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.styles.OmniboxDrawableState;
import org.chromium.chrome.browser.omnibox.styles.OmniboxImageSupplier;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties;
import org.chromium.components.omnibox.AnswerTypeProto.AnswerType;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.Optional;

/** A class that handles model and view creation for the most commonly used omnibox suggestion. */
public class AnswerSuggestionProcessor extends BaseSuggestionViewProcessor {
    private static final String COLOR_REVERSAL_COUNTRY_LIST = "ja-JP,ko-KR,zh-CN,zh-TW";

    private final UrlBarEditingTextStateProvider mUrlBarEditingTextProvider;

    public AnswerSuggestionProcessor(
            @NonNull Context context,
            @NonNull SuggestionHost suggestionHost,
            @NonNull UrlBarEditingTextStateProvider editingTextProvider,
            @NonNull Optional<OmniboxImageSupplier> imageSupplier) {
        super(context, suggestionHost, imageSupplier);
        mUrlBarEditingTextProvider = editingTextProvider;
    }

    @Override
    public boolean doesProcessSuggestion(@NonNull AutocompleteMatch suggestion, int position) {
        // Calculation answers are specific in a way that these are basic suggestions, but processed
        // as answers, when new answer layout is enabled.
        return suggestion.getAnswerTemplate() != null
                || suggestion.hasAnswer()
                || suggestion.getType() == OmniboxSuggestionType.CALCULATOR;
    }

    @Override
    public int getViewTypeId() {
        return OmniboxSuggestionUiType.ANSWER_SUGGESTION;
    }

    @Override
    public @NonNull PropertyModel createModel() {
        return new PropertyModel(AnswerSuggestionViewProperties.ALL_KEYS);
    }

    @Override
    public void populateModel(
            @NonNull AutocompleteMatch suggestion, @NonNull PropertyModel model, int position) {
        super.populateModel(suggestion, model, position);
        setStateForSuggestion(model, suggestion, position);
    }

    private void setStateForSuggestion(
            PropertyModel model, AutocompleteMatch suggestion, int position) {
        AnswerType answerType =
                suggestion.getAnswer() == null
                        ? suggestion.getAnswerType()
                        : suggestion.getAnswer().getType();
        boolean suggestionTextColorReversal = checkColorReversalRequired(answerType);
        AnswerText[] details;
        boolean shouldShowCardUi = false;
        model.set(BaseSuggestionViewProperties.TOP_PADDING, 0);
        model.set(AnswerSuggestionViewProperties.RIGHT_PADDING, 0);
        if (suggestion.getAnswerTemplate() != null) {
            shouldShowCardUi =
                    OmniboxFeatures.shouldShowRichAnswerCard()
                            && suggestion.getActions().size() > 0;
            details =
                    RichAnswerText.from(
                            mContext,
                            suggestion.getAnswerTemplate(),
                            answerType,
                            suggestionTextColorReversal,
                            shouldShowCardUi);

            model.set(BaseSuggestionViewProperties.USE_LARGE_DECORATION, shouldShowCardUi);
            if (shouldShowCardUi) {
                int leadInSpacing =
                        mContext.getResources()
                                .getDimensionPixelSize(R.dimen.omnibox_simple_card_leadin);
                model.set(BaseSuggestionViewProperties.ACTION_CHIP_LEAD_IN_SPACING, leadInSpacing);
                model.set(
                        BaseSuggestionViewProperties.TOP_PADDING,
                        mContext.getResources()
                                .getDimensionPixelSize(R.dimen.omnibox_simple_card_top_padding));
                model.set(AnswerSuggestionViewProperties.RIGHT_PADDING, leadInSpacing);
            }
        } else {
            details =
                    AnswerTextNewLayout.from(
                            mContext,
                            suggestion,
                            mUrlBarEditingTextProvider.getTextWithoutAutocomplete(),
                            suggestionTextColorReversal);
        }

        model.set(AnswerSuggestionViewProperties.TEXT_LINE_1_TEXT, details[0].getText());
        model.set(AnswerSuggestionViewProperties.TEXT_LINE_2_TEXT, details[1].getText());

        model.set(
                AnswerSuggestionViewProperties.TEXT_LINE_1_ACCESSIBILITY_DESCRIPTION,
                details[0].getAccessibilityDescription());
        model.set(
                AnswerSuggestionViewProperties.TEXT_LINE_2_ACCESSIBILITY_DESCRIPTION,
                details[1].getAccessibilityDescription());

        model.set(AnswerSuggestionViewProperties.TEXT_LINE_1_MAX_LINES, details[0].getMaxLines());
        model.set(AnswerSuggestionViewProperties.TEXT_LINE_2_MAX_LINES, details[1].getMaxLines());

        if (shouldShowCardUi) {
            setActionButtons(model, null);
        } else {
            setTabSwitchOrRefineAction(model, suggestion, position);
        }
        if (suggestion.hasAnswer() && suggestion.getAnswer().getSecondLine().hasImage()) {
            fetchImage(model, new GURL(suggestion.getAnswer().getSecondLine().getImage()));
        } else if (suggestion.getAnswerTemplate() != null) {
            GURL imageUrl =
                    suggestion.getAnswerTemplate().getAnswers(0).hasImage()
                            ? new GURL(
                                    suggestion
                                            .getAnswerTemplate()
                                            .getAnswers(0)
                                            .getImage()
                                            .getUrl())
                            : new GURL("");
            if (imageUrl.isValid()) {
                fetchImage(
                        model,
                        new GURL(suggestion.getAnswerTemplate().getAnswers(0).getImage().getUrl()));
            } else if (shouldShowCardUi) {
                // The card ui should not show fallback images; if there is not an answer-specific
                // image, there should be no decoration at all.
                model.set(BaseSuggestionViewProperties.SHOW_DECORATION, false);
                setOmniboxDrawableState(model, null);
            }
        }
    }

    /**
     * Checks if we need to apply color reversion on the answer suggestion.
     *
     * @param answerType The type of a suggested answer.
     * @return true, if red/green colors should be swapped.
     */
    @VisibleForTesting
    public boolean checkColorReversalRequired(AnswerType answerType) {
        boolean isFinanceAnswer = answerType == AnswerType.ANSWER_TYPE_FINANCE;
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
    public @NonNull OmniboxDrawableState getFallbackIcon(@NonNull AutocompleteMatch suggestion) {
        int icon = 0;

        AnswerType type =
                suggestion.getAnswer() == null
                        ? suggestion.getAnswerType()
                        : suggestion.getAnswer().getType();
        if (type == null) {
            type = AnswerType.ANSWER_TYPE_UNSPECIFIED;
        }
        if (type != AnswerType.ANSWER_TYPE_UNSPECIFIED) {
            switch (type) {
                case ANSWER_TYPE_DICTIONARY:
                    icon = R.drawable.ic_book_round;
                    break;
                case ANSWER_TYPE_FINANCE:
                    icon = R.drawable.ic_swap_vert_round;
                    break;
                case ANSWER_TYPE_GENERIC_ANSWER:
                case ANSWER_TYPE_SPORTS:
                    icon = R.drawable.ic_google_round;
                    break;
                case ANSWER_TYPE_SUNRISE_SUNSET:
                    icon = R.drawable.ic_wb_sunny_round;
                    break;
                case ANSWER_TYPE_TRANSLATION:
                    icon = R.drawable.logo_translate_round;
                    break;
                case ANSWER_TYPE_WEATHER:
                    icon = R.drawable.logo_partly_cloudy;
                    break;
                case ANSWER_TYPE_WHEN_IS:
                    icon = R.drawable.ic_event_round;
                    break;
                case ANSWER_TYPE_CURRENCY:
                    icon = R.drawable.ic_loop_round;
                    break;
                default:
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
