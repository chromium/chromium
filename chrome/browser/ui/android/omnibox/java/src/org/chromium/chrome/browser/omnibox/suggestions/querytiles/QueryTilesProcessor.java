// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.querytiles;

import android.content.Context;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.omnibox.styles.OmniboxImageSupplier;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.carousel.BaseCarouselSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.carousel.BaseCarouselSuggestionViewProperties;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;

/** SuggestionProcessor for Query Tiles. */
public class QueryTilesProcessor extends BaseCarouselSuggestionProcessor {
    private final @NonNull SuggestionHost mSuggestionHost;
    private final @Nullable OmniboxImageSupplier mImageSupplier;

    /**
     * Constructor.
     *
     * @param context An Android context.
     * @param host SuggestionHost receiving notifications about user actions.
     * @param imageSupplier Class retrieving favicons for the MV Tiles.
     */
    public QueryTilesProcessor(
            @NonNull Context context,
            @NonNull SuggestionHost host,
            @Nullable OmniboxImageSupplier imageSupplier) {
        super(context);
        mSuggestionHost = host;
        mImageSupplier = imageSupplier;
    }

    @Override
    public boolean doesProcessSuggestion(AutocompleteMatch match, int matchIndex) {
        return match.getType() == OmniboxSuggestionType.TILE_SUGGESTION;
    }

    @Override
    public int getViewTypeId() {
        return OmniboxSuggestionUiType.QUERY_TILES;
    }

    @Override
    public PropertyModel createModel() {
        return new PropertyModel.Builder(BaseCarouselSuggestionViewProperties.ALL_KEYS)
                .with(BaseCarouselSuggestionViewProperties.TILES, new ArrayList<>())
                .build();
    }

    @Override
    public int getMinimumCarouselItemViewHeight() {
        // TODO(crbug/1490333): identify correct height.
        return 0;
    }

    @Override
    public void populateModel(AutocompleteMatch match, PropertyModel model, int matchIndex) {
        super.populateModel(match, model, matchIndex);
    }
}
