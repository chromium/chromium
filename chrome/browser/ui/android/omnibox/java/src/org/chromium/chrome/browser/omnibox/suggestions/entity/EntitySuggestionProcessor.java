// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.entity;

import android.content.Context;
import android.graphics.Color;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.styles.OmniboxDrawableState;
import org.chromium.chrome.browser.omnibox.styles.OmniboxImageSupplier;
import org.chromium.chrome.browser.omnibox.styles.SuggestionSpannable;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.basic.BasicSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewProperties;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Optional;

/** A class that handles model and view creation for the Entity suggestions. */
public class EntitySuggestionProcessor extends BasicSuggestionProcessor {
    public EntitySuggestionProcessor(
            @NonNull Context context,
            @NonNull SuggestionHost suggestionHost,
            @NonNull UrlBarEditingTextStateProvider editingTextProvider,
            @NonNull Optional<OmniboxImageSupplier> imageSupplier,
            @NonNull BookmarkState bookmarkState) {
        super(context, suggestionHost, editingTextProvider, imageSupplier, bookmarkState);
    }

    @Override
    public boolean doesProcessSuggestion(@NonNull AutocompleteMatch suggestion, int position) {
        // TODO(ender): Expand with Categorical Suggestions once these get their dedicated type:
        // - Confirm whether custom handling applicable to Entities should also be applied to
        //   Categorical Suggestions,
        // - Return null upon call to getSuggestionDescription(), unless Categorical Suggestions
        //   make proper use of the description text. Do not show <Search with X>.
        return suggestion.getType() == OmniboxSuggestionType.SEARCH_SUGGEST_ENTITY;
    }

    @Override
    public int getViewTypeId() {
        return OmniboxSuggestionUiType.ENTITY_SUGGESTION;
    }

    @Override
    public @NonNull PropertyModel createModel() {
        return new PropertyModel(EntitySuggestionViewProperties.ALL_KEYS);
    }

    @Override
    public void populateModel(
            @NonNull AutocompleteMatch suggestion, @NonNull PropertyModel model, int position) {
        super.populateModel(suggestion, model, position);
        model.set(SuggestionViewProperties.ALLOW_WRAP_AROUND, false);
    }

    @VisibleForTesting
    @Override
    public @NonNull OmniboxDrawableState getFallbackIcon(@NonNull AutocompleteMatch match) {
        if (OmniboxFeatures.isLowMemoryDevice()) return super.getFallbackIcon(match);

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
    protected @Nullable SuggestionSpannable getSuggestionDescription(AutocompleteMatch match) {
        return new SuggestionSpannable(match.getDescription());
    }
}
