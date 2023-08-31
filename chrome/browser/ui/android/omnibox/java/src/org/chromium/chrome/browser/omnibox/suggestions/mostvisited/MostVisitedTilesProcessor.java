// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.mostvisited;

import android.content.Context;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

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
    private final @NonNull Context mContext;
    private final @NonNull SuggestionHost mSuggestionHost;
    private final @Nullable OmniboxImageSupplier mImageSupplier;
    private final int mMinCarouselItemViewHeight;

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
    public boolean doesProcessSuggestion(AutocompleteMatch match, int matchIndex) {
        return match.getType() == OmniboxSuggestionType.TILE_NAVSUGGEST;
    }

    @Override
    public int getViewTypeId() {
        return OmniboxSuggestionUiType.TILE_NAVSUGGEST;
    }

    @Override
    public PropertyModel createModel() {
        return new PropertyModel.Builder(BaseCarouselSuggestionViewProperties.ALL_KEYS)
                .with(BaseCarouselSuggestionViewProperties.TILES, new ArrayList<>())
                .build();
    }

    @Override
    public int getMinimumCarouselItemViewHeight() {
        return mMinCarouselItemViewHeight;
    }

    @Override
    public void populateModel(AutocompleteMatch match, PropertyModel model, int matchIndex) {
        super.populateModel(match, model, matchIndex);

        // Note: we should never show most visited tiles in incognito mode. Catch this early
        // if we ever do.
        assert model.get(SuggestionCommonProperties.COLOR_SCHEME) != BrandedColorScheme.INCOGNITO;

        if (match.getType() == OmniboxSuggestionType.TILE_NAVSUGGEST) {
            updateModelFromTileNavsuggest(match, model, matchIndex);
        } else {
            assert false : "Unsupported AutocompleteMatch type: " + match.getType();
        }
    }

    private void updateModelFromTileNavsuggest(
            AutocompleteMatch match, PropertyModel model, int matchIndex) {
        List<AutocompleteMatch.SuggestTile> tiles = match.getSuggestTiles();
        int tilesCount = tiles.size();
        List<ListItem> tileList = model.get(BaseCarouselSuggestionViewProperties.TILES);

        for (int elementIndex = 0; elementIndex < tilesCount; elementIndex++) {
            SuggestTile tile = tiles.get(elementIndex);
            int index = elementIndex;
            // Use website host text when the website title is empty (for example: gmail.com).
            String title = TextUtils.isEmpty(tile.title) ? tile.url.getHost() : tile.title;

            // clang-format off
            PropertyModel tileModel = createTile(title, tile.url, tile.isSearch,
                    v -> {
                        OmniboxMetrics.recordSuggestTileTypeUsed(index, tile.isSearch);
                        mSuggestionHost.onSuggestionClicked(match, matchIndex, tile.url);
                    },
                    v -> {
                        mSuggestionHost.onDeleteMatchElement(match, title, index);
                        return true;
                    });
            // clang-format on

            tileList.add(new ListItem(
                    BaseCarouselSuggestionItemViewBuilder.ViewType.TILE_VIEW, tileModel));
        }
    }

    private PropertyModel createTile(String title, GURL url, boolean isSearch,
            View.OnClickListener onClick, View.OnLongClickListener onLongClick) {
        String contentDescription;
        Drawable decoration;

        if (isSearch) {
            decoration = OmniboxResourceProvider.getDrawable(
                    mContext, R.drawable.ic_suggestion_magnifier);
            contentDescription = OmniboxResourceProvider.getString(
                    mContext, R.string.accessibility_omnibox_most_visited_tile_search, title);
        } else {
            decoration = OmniboxResourceProvider.getDrawable(mContext, R.drawable.ic_globe_24dp);
            contentDescription = OmniboxResourceProvider.getString(mContext,
                    R.string.accessibility_omnibox_most_visited_tile_navigate, title,
                    url.getHost());
        }

        var model = new PropertyModel.Builder(TileViewProperties.ALL_KEYS)
                            .with(TileViewProperties.TITLE, title)
                            .with(TileViewProperties.TITLE_LINES, 1)
                            .with(TileViewProperties.ON_FOCUS_VIA_SELECTION,
                                    () -> mSuggestionHost.setOmniboxEditingText(url.getSpec()))
                            .with(TileViewProperties.ON_CLICK, onClick)
                            .with(TileViewProperties.ON_LONG_CLICK, onLongClick)
                            .with(TileViewProperties.ICON_TINT,
                                    ChromeColors.getSecondaryIconTint(
                                            mContext, /* isIncognito= */ false))
                            .with(TileViewProperties.CONTENT_DESCRIPTION, contentDescription)
                            .with(TileViewProperties.ICON, decoration)
                            .with(TileViewProperties.SMALL_ICON_ROUNDING_RADIUS,
                                    mContext.getResources().getDimensionPixelSize(
                                            R.dimen.omnibox_small_icon_rounding_radius))
                            .build();

        // Fetch site favicon for MV tiles.
        if (!isSearch && mImageSupplier != null) {
            mImageSupplier.fetchFavicon(url, icon -> {
                if (icon == null) {
                    mImageSupplier.generateFavicon(url, fallback -> {
                        model.set(TileViewProperties.ICON, new BitmapDrawable(fallback));
                        model.set(TileViewProperties.ICON_TINT, null);
                    });
                    return;
                }
                model.set(TileViewProperties.ICON, new BitmapDrawable(icon));
                model.set(TileViewProperties.ICON_TINT, null);
            });
        }

        return model;
    }
}
