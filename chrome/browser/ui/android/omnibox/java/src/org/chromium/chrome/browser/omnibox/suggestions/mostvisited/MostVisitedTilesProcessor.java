// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.mostvisited;

import android.content.Context;
import android.graphics.drawable.BitmapDrawable;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView.RecycledViewPool;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.OmniboxFeatures;
import org.chromium.chrome.browser.omnibox.OmniboxMetrics;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxImageSupplier;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.carousel.BaseCarouselSuggestionItemViewBuilder;
import org.chromium.chrome.browser.omnibox.suggestions.carousel.BaseCarouselSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.carousel.BaseCarouselSuggestionViewProperties;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.tile.TileViewProperties;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatch.SuggestTile;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * SuggestionProcessor for Most Visited URL tiles.
 */
public class MostVisitedTilesProcessor extends BaseCarouselSuggestionProcessor {
    /**
     * RecyclerView pool that adds the max recycled view for the most visited tile carousel and
     * avoid re-creating views until we're sure we won't be needing them.
     */
    private class MostVisitedTilesRecycledViewPool extends RecycledViewPool {
        public MostVisitedTilesRecycledViewPool() {
            setMaxRecycledViews(OmniboxSuggestionUiType.DEFAULT, 10);
        }
    }

    private final @NonNull Context mContext;
    private final @NonNull SuggestionHost mSuggestionHost;
    private final @Nullable OmniboxImageSupplier mImageSupplier;
    private final int mMinCarouselItemViewHeight;
    private @Nullable RecycledViewPool mMostVisitedTilesRecycledViewPool;
    private boolean mEnableOrganicRepeatableQueries;

    /**
     * Constructor.
     *
     * @param context An Android context.
     * @param host SuggestionHost receiving notifications about user actions.
     * @param imageSupplier Class retrieving favicons for the MV Tiles.
     */
    public MostVisitedTilesProcessor(@NonNull Context context, @NonNull SuggestionHost host,
            @Nullable OmniboxImageSupplier imageSupplier) {
        super(context);
        mContext = context;
        mSuggestionHost = host;
        mImageSupplier = imageSupplier;
        mMinCarouselItemViewHeight =
                mContext.getResources().getDimensionPixelSize(R.dimen.tile_view_min_height);
    }

    @Override
    public boolean doesProcessSuggestion(AutocompleteMatch suggestion, int matchIndex) {
        return suggestion.getType() == OmniboxSuggestionType.TILE_NAVSUGGEST;
    }

    @Override
    public int getViewTypeId() {
        return OmniboxSuggestionUiType.TILE_NAVSUGGEST;
    }

    @Override
    public PropertyModel createModel() {
        return new PropertyModel(BaseCarouselSuggestionViewProperties.ALL_KEYS);
    }

    @Override
    public int getMinimumCarouselItemViewHeight() {
        return mMinCarouselItemViewHeight;
    }

    @Override
    public void onNativeInitialized() {
        super.onNativeInitialized();

        mEnableOrganicRepeatableQueries =
                ChromeFeatureList.isEnabled(ChromeFeatureList.HISTORY_ORGANIC_REPEATABLE_QUERIES);

        // Initialize a recycled view pool for the most visited tiles carousel to reduce unnecessary
        // fetching and jankiness.
        if (OmniboxFeatures.shouldAddMostVisitedTilesRecycledViewPool()) {
            mMostVisitedTilesRecycledViewPool = new MostVisitedTilesRecycledViewPool();
        }
    }

    @Override
    public void populateModel(AutocompleteMatch suggestion, PropertyModel model, int matchIndex) {
        super.populateModel(suggestion, model, matchIndex);

        final List<AutocompleteMatch.SuggestTile> tiles = suggestion.getSuggestTiles();
        final int tilesCount = tiles.size();
        final List<ListItem> tileList = new ArrayList<>(tilesCount);

        for (int elementIndex = 0; elementIndex < tilesCount; elementIndex++) {
            final PropertyModel tileModel = new PropertyModel(TileViewProperties.ALL_KEYS);
            final SuggestTile tile = tiles.get(elementIndex);
            // Use website host text when the website title is empty (for example: gmail.com).
            final String title = TextUtils.isEmpty(tile.title) ? tile.url.getHost() : tile.title;
            final GURL url = tile.url;
            final boolean isSearch = tile.isSearch && mEnableOrganicRepeatableQueries;
            final int itemIndex = elementIndex;

            tileModel.set(TileViewProperties.TITLE, title);
            tileModel.set(TileViewProperties.TITLE_LINES, 1);
            tileModel.set(TileViewProperties.ON_FOCUS_VIA_SELECTION,
                    () -> mSuggestionHost.setOmniboxEditingText(url.getSpec()));
            tileModel.set(TileViewProperties.ON_CLICK, v -> {
                OmniboxMetrics.recordSuggestTileTypeUsed(itemIndex, isSearch);
                mSuggestionHost.onSuggestionClicked(suggestion, matchIndex, url);
            });

            final int elementIndexForDeletion = elementIndex;
            tileModel.set(TileViewProperties.ON_LONG_CLICK, v -> {
                mSuggestionHost.onDeleteMatchElement(
                        suggestion, title, matchIndex, elementIndexForDeletion);
                return true;
            });

            tileModel.set(TileViewProperties.ICON_TINT,
                    ChromeColors.getSecondaryIconTint(mContext, /* isIncognito= */ false));
            if (isSearch) {
                // Note: we should never show most visited tiles in incognito mode. Catch this early
                // if we ever do.
                assert model.get(SuggestionCommonProperties.COLOR_SCHEME)
                        != BrandedColorScheme.INCOGNITO;
                tileModel.set(TileViewProperties.ICON,
                        OmniboxResourceProvider.getDrawable(
                                mContext, R.drawable.ic_suggestion_magnifier));
                tileModel.set(TileViewProperties.CONTENT_DESCRIPTION,
                        OmniboxResourceProvider.getString(mContext,
                                R.string.accessibility_omnibox_most_visited_tile_search, title));
            } else {
                tileModel.set(TileViewProperties.ICON,
                        OmniboxResourceProvider.getDrawable(mContext, R.drawable.ic_globe_24dp));
                tileModel.set(TileViewProperties.CONTENT_DESCRIPTION,
                        OmniboxResourceProvider.getString(mContext,
                                R.string.accessibility_omnibox_most_visited_tile_navigate, title,
                                url.getHost()));

                tileModel.set(TileViewProperties.SMALL_ICON_ROUNDING_RADIUS,
                        mContext.getResources().getDimensionPixelSize(
                                R.dimen.omnibox_small_icon_rounding_radius));
                if (mImageSupplier != null) {
                    mImageSupplier.fetchFavicon(url, icon -> {
                        if (icon == null) {
                            mImageSupplier.generateFavicon(url, fallback -> {
                                tileModel.set(
                                        TileViewProperties.ICON, new BitmapDrawable(fallback));
                                tileModel.set(TileViewProperties.ICON_TINT, null);
                            });
                            return;
                        }
                        tileModel.set(TileViewProperties.ICON, new BitmapDrawable(icon));
                        tileModel.set(TileViewProperties.ICON_TINT, null);
                    });
                }
            }

            tileList.add(new ListItem(
                    BaseCarouselSuggestionItemViewBuilder.ViewType.TILE_VIEW, tileModel));
        }

        model.set(BaseCarouselSuggestionViewProperties.TILES, tileList);
        model.set(BaseCarouselSuggestionViewProperties.RECYCLED_VIEW_POOL,
                mMostVisitedTilesRecycledViewPool);
    }

    /**
     * Respond to URL bar focus change.
     *
     * @param hasFocus Indicates whether URL bar is now focused.
     */
    @Override
    public void onUrlFocusChange(boolean hasFocus) {
        // Clear the Recycled View Pool when the omnibox loses focus.
        if (!hasFocus && mMostVisitedTilesRecycledViewPool != null) {
            mMostVisitedTilesRecycledViewPool.clear();
        }
    }
}
