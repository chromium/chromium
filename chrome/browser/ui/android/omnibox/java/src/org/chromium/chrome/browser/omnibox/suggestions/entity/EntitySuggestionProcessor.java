// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.entity;

import android.content.Context;
import android.graphics.Color;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.omnibox.styles.OmniboxDrawableState;
import org.chromium.chrome.browser.omnibox.styles.OmniboxImageSupplier;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProcessor;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.ui.modelutil.PropertyModel;

/** A class that handles model and view creation for the Entity suggestions. */
public class EntitySuggestionProcessor extends BaseSuggestionViewProcessor {
    /**
     * @param context An Android context.
     * @param suggestionHost A handle to the object using the suggestions.
     * @param imageSupplier Supplier used to retrieve suggestion images.
     */
    public EntitySuggestionProcessor(
            Context context, SuggestionHost suggestionHost, OmniboxImageSupplier imageSupplier) {
        super(context, suggestionHost, imageSupplier);
    }

    @Override
    public boolean doesProcessSuggestion(AutocompleteMatch suggestion, int position) {
        return suggestion.getType() == OmniboxSuggestionType.SEARCH_SUGGEST_ENTITY;
    }

    @Override
    public int getViewTypeId() {
        return OmniboxSuggestionUiType.ENTITY_SUGGESTION;
    }

    @Override
    public PropertyModel createModel() {
        return new PropertyModel(EntitySuggestionViewProperties.ALL_KEYS);
    }

    @VisibleForTesting
    @Override
    public OmniboxDrawableState getFallbackIcon(AutocompleteMatch match) {
        var colorSpec = match.getImageDominantColor();
        if (TextUtils.isEmpty(colorSpec)) return super.getFallbackIcon(match);

        try {
            int color = Color.parseColor(colorSpec);
            return OmniboxDrawableState.forColor(color);
        } catch (IllegalArgumentException e) {
            return super.getFallbackIcon(match);
        }
    }

    @Override
    public void populateModel(AutocompleteMatch suggestion, PropertyModel model, int position) {
        super.populateModel(suggestion, model, position);
        model.set(EntitySuggestionViewProperties.SUBJECT_TEXT, suggestion.getDisplayText());
        model.set(EntitySuggestionViewProperties.DESCRIPTION_TEXT, suggestion.getDescription());
        setTabSwitchOrRefineAction(model, suggestion, position);
    }
}
