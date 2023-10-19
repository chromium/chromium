// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.querytiles;

import android.content.Context;
import android.graphics.drawable.BitmapDrawable;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxImageSupplier;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.carousel.BaseCarouselSuggestionItemViewBuilder;
import org.chromium.chrome.browser.omnibox.suggestions.carousel.BaseCarouselSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.carousel.BaseCarouselSuggestionViewProperties;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/** SuggestionProcessor for Query Tiles. */
public class QueryTilesProcessor extends BaseCarouselSuggestionProcessor {
    private final @NonNull SuggestionHost mSuggestionHost;
    private final @Nullable OmniboxImageSupplier mImageSupplier;
    private final int mCarouselItemViewWidth;
    private final int mCarouselItemViewHeight;

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
        mCarouselItemViewWidth =
                mContext.getResources().getDimensionPixelSize(R.dimen.query_tile_view_width);
        mCarouselItemViewHeight =
                mContext.getResources().getDimensionPixelSize(R.dimen.query_tile_view_height);
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
    public int getCarouselItemViewWidth() {
        return mCarouselItemViewWidth;
    }

    @Override
    public int getCarouselItemViewHeight() {
        return mCarouselItemViewHeight;
    }

    @Override
    public void populateModel(AutocompleteMatch match, PropertyModel model, int matchIndex) {
        super.populateModel(match, model, matchIndex);

        List<ListItem> tileList = model.get(BaseCarouselSuggestionViewProperties.TILES);
        var tileModel =
                new PropertyModel.Builder(QueryTileViewProperties.ALL_UNIQUE_KEYS)
                        .with(QueryTileViewProperties.TITLE, match.getDisplayText())
                        .with(
                                QueryTileViewProperties.ON_FOCUS_VIA_SELECTION,
                                () ->
                                        mSuggestionHost.setOmniboxEditingText(
                                                match.getFillIntoEdit()))
                        .with(
                                QueryTileViewProperties.ON_CLICK,
                                v ->
                                        mSuggestionHost.onSuggestionClicked(
                                                match, matchIndex, match.getUrl()))
                        .build();
        tileList.add(
                new ListItem(BaseCarouselSuggestionItemViewBuilder.ViewType.QUERY_TILE, tileModel));

        if (mImageSupplier != null && match.getImageUrl().isValid()) {
            mImageSupplier.fetchImage(
                    match.getImageUrl(),
                    image ->
                            tileModel.set(
                                    QueryTileViewProperties.IMAGE,
                                    new BitmapDrawable(mContext.getResources(), image)));
        }
    }
}
