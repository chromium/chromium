// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.answer;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.styles.OmniboxDrawableState;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteUIContext;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.ui.modelutil.PropertyModel;

/** A class that handles model and view creation for calculator answers. */
@NullMarked
public class AnswerSuggestionProcessor extends BaseSuggestionViewProcessor {

    private final UrlBarEditingTextStateProvider mUrlBarEditingTextProvider;

    /**
     * Constructor using AutocompleteUIContext for common dependencies.
     *
     * @param uiContext Context object containing common UI dependencies.
     */
    public AnswerSuggestionProcessor(AutocompleteUIContext uiContext) {
        super(uiContext);
        mUrlBarEditingTextProvider = uiContext.textProvider;
    }

    @Override
    public boolean doesProcessSuggestion(AutocompleteMatch suggestion, int position) {
        // Calculation answers are specific in a way that these are basic suggestions, but processed
        // as answers, when new answer layout is enabled.
        return suggestion.getType() == OmniboxSuggestionType.CALCULATOR;
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
    public void populateModel(
            AutocompleteInput input,
            AutocompleteMatch suggestion,
            PropertyModel model,
            int position) {
        super.populateModel(input, suggestion, model, position);
        setStateForSuggestion(model, input, suggestion, position);
    }

    private void setStateForSuggestion(
            PropertyModel model,
            AutocompleteInput input,
            AutocompleteMatch suggestion,
            int position) {
        AnswerText[] details;
        model.set(BaseSuggestionViewProperties.TOP_PADDING, 0);
        model.set(AnswerSuggestionViewProperties.RIGHT_PADDING, 0);
        details =
                CalculatorAnswerTextLayout.from(
                        mContext,
                        suggestion,
                        mUrlBarEditingTextProvider.getTextWithoutAutocomplete());

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

        setRemoveOrRefineAction(model, input, suggestion, position);
    }

    @Override
    public OmniboxDrawableState getFallbackIcon(AutocompleteMatch suggestion) {
        int icon = 0;
        if (suggestion.getType() == OmniboxSuggestionType.CALCULATOR) {
            icon = R.drawable.ic_equals_sign_round;
        }

        return icon == 0
                ? super.getFallbackIcon(suggestion)
                : OmniboxDrawableState.forLargeIcon(
                        mUiContext.resourceProvider, icon, /* allowTint= */ false);
    }
}
