// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.tail;

import android.content.Context;

import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.base.SuggestionDrawableState;
import org.chromium.chrome.browser.omnibox.suggestions.base.SuggestionSpannable;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.PropertyModel;

/** A class that handles model and view creation for the tail suggestions. */
public class TailSuggestionProcessor extends BaseSuggestionViewProcessor {
    private final boolean mAlignTailSuggestions;
    private AlignmentManager mAlignmentManager;

    /**
     * @param context An Android context.
     * @param suggestionHost A handle to the object using the suggestions.
     */
    public TailSuggestionProcessor(Context context, SuggestionHost suggestionHost) {
        super(context, suggestionHost, null);
        mAlignTailSuggestions = DeviceFormFactor.isNonMultiDisplayContextOnTablet(context);
    }

    @Override
    public boolean doesProcessSuggestion(AutocompleteMatch suggestion, int position) {
        return mAlignTailSuggestions
                && suggestion.getType() == OmniboxSuggestionType.SEARCH_SUGGEST_TAIL;
    }

    @Override
    public int getViewTypeId() {
        return OmniboxSuggestionUiType.TAIL_SUGGESTION;
    }

    @Override
    public PropertyModel createModel() {
        return new PropertyModel(TailSuggestionViewProperties.ALL_KEYS);
    }

    @Override
    public void populateModel(AutocompleteMatch suggestion, PropertyModel model, int position) {
        super.populateModel(suggestion, model, position);

        assert mAlignmentManager != null;

        model.set(TailSuggestionViewProperties.ALIGNMENT_MANAGER, mAlignmentManager);
        model.set(TailSuggestionViewProperties.FILL_INTO_EDIT, suggestion.getFillIntoEdit());

        final SuggestionSpannable text = new SuggestionSpannable(suggestion.getDisplayText());
        applyHighlightToMatchRegions(text, suggestion.getDisplayTextClassifications());
        model.set(TailSuggestionViewProperties.TEXT, text);

        setSuggestionDrawableState(model,
                SuggestionDrawableState.Builder
                        .forDrawableRes(getContext(), R.drawable.ic_suggestion_magnifier)
                        .setAllowTint(true)
                        .build());
        setTabSwitchOrRefineAction(model, suggestion, position);
    }

    @Override
    public void onSuggestionsReceived() {
        mAlignmentManager = new AlignmentManager();
    }
}
