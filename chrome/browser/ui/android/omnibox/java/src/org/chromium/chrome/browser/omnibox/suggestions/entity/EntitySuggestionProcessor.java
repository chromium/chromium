// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.entity;

import android.content.Context;
import android.graphics.Color;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxDrawableState;
import org.chromium.chrome.browser.omnibox.styles.OmniboxImageSupplier;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProcessor;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/** A class that handles model and view creation for the Entity suggestions. */
public class EntitySuggestionProcessor extends BaseSuggestionViewProcessor {
    private final OmniboxImageSupplier mImageSupplier;

    /**
     * @param context An Android context.
     * @param suggestionHost A handle to the object using the suggestions.
     * @param imageSupplier Supplier used to retrieve suggestion images.
     */
    public EntitySuggestionProcessor(
            Context context, SuggestionHost suggestionHost, OmniboxImageSupplier imageSupplier) {
        super(context, suggestionHost, null);
        mImageSupplier = imageSupplier;
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

    private void fetchEntityImage(GURL imageUrl, PropertyModel model) {
        mImageSupplier.fetchImage(imageUrl, bitmap -> {
            setOmniboxDrawableState(model,
                    OmniboxDrawableState.Builder.forBitmap(mContext, bitmap)
                            .setUseRoundedCorners(true)
                            .setLarge(true)
                            .build());
        });
    }

    @VisibleForTesting
    public void applyImageDominantColor(String colorSpec, PropertyModel model) {
        if (TextUtils.isEmpty(colorSpec)) {
            return;
        }

        int color;
        try {
            color = Color.parseColor(colorSpec);
        } catch (IllegalArgumentException e) {
            // The supplied color information could not be parsed.
            return;
        }

        setOmniboxDrawableState(model,
                OmniboxDrawableState.Builder.forColor(color)
                        .setLarge(true)
                        .setUseRoundedCorners(true)
                        .build());
    }

    @Override
    public void populateModel(AutocompleteMatch suggestion, PropertyModel model, int position) {
        super.populateModel(suggestion, model, position);
        setOmniboxDrawableState(model,
                OmniboxDrawableState.Builder
                        .forDrawableRes(mContext, R.drawable.ic_suggestion_magnifier)
                        .setAllowTint(true)
                        .build());

        if (mImageSupplier != null) {
            applyImageDominantColor(suggestion.getImageDominantColor(), model);
            fetchEntityImage(suggestion.getImageUrl(), model);
        }

        model.set(EntitySuggestionViewProperties.SUBJECT_TEXT, suggestion.getDisplayText());
        model.set(EntitySuggestionViewProperties.DESCRIPTION_TEXT, suggestion.getDescription());
        setTabSwitchOrRefineAction(model, suggestion, position);
    }
}
