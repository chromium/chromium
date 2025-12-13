// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.mostvisited;

import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.OmniboxMetrics;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxImageSupplier;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteUIContext;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.base.DynamicSpacingRecyclerViewItemDecoration;
import org.chromium.chrome.browser.omnibox.suggestions.carousel.BaseCarouselSuggestionItemViewBuilder;
import org.chromium.chrome.browser.omnibox.suggestions.carousel.BaseCarouselSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.carousel.BaseCarouselSuggestionViewProperties;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.tile.TileViewProperties;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** SuggestionProcessor for Most Visited URL tiles. */
@NullMarked
public class MostVisitedTilesProcessor extends BaseCarouselSuggestionProcessor {
    private final SuggestionHost mSuggestionHost;
    private final @Nullable OmniboxImageSupplier mImageSupplier;
    private final @Px int mCarouselItemViewWidth;
    private final @Px int mCarouselItemViewHeight;
    private final @Px int mInitialSpacing;
    private final @Px int mElementSpacing;

    /**
     * @param uiContext Context object containing common UI dependencies.
     */
    public MostVisitedTilesProcessor(AutocompleteUIContext uiContext) {
        super(uiContext.context);
        mSuggestionHost = uiContext.host;
        mImageSupplier = uiContext.imageSupplier;
        mCarouselItemViewWidth =
                mContext.getResources().getDimensionPixelSize(R.dimen.tile_view_width);
        mCarouselItemViewHeight =
                mContext.getResources().getDimensionPixelSize(R.dimen.tile_view_min_height);

        mInitialSpacing =
                OmniboxResourceProvider.getHeaderStartPadding(uiContext.context)
                        - uiContext
                                .context
                                .getResources()
                                .getDimensionPixelSize(R.dimen.tile_view_padding);
        mElementSpacing =
                uiContext
                        .context
                        .getResources()
                        .getDimensionPixelSize(
                                R.dimen.omnibox_carousel_suggestion_minimum_item_spacing);
    }

    @Override
    public boolean doesProcessSuggestion(AutocompleteMatch match, int matchIndex) {
        switch (match.getType()) {
            case OmniboxSuggestionType.TILE_MOST_VISITED_SITE:
            case OmniboxSuggestionType.TILE_REPEATABLE_QUERY:
                return true;
            default:
                return false;
        }
    }

    @Override
    public int getViewTypeId() {
        return OmniboxSuggestionUiType.TILE_NAVSUGGEST;
    }

    @Override
    public PropertyModel createModel() {
        PropertyModel model =
                new PropertyModel.Builder(BaseCarouselSuggestionViewProperties.ALL_KEYS)
                        .with(BaseCarouselSuggestionViewProperties.TILES, new ArrayList<>())
                        .with(
                                BaseCarouselSuggestionViewProperties.CONTENT_DESCRIPTION,
                                mContext.getString(
                                        R.string.accessibility_omnibox_most_visited_list))
                        .with(
                                BaseCarouselSuggestionViewProperties.TOP_PADDING,
                                OmniboxResourceProvider.getMostVisitedCarouselTopPadding(mContext))
                        .with(
                                BaseCarouselSuggestionViewProperties.BOTTOM_PADDING,
                                OmniboxResourceProvider.getMostVisitedCarouselBottomPadding(
                                        mContext))
                        .with(BaseCarouselSuggestionViewProperties.APPLY_BACKGROUND, false)
                        .with(
                                BaseCarouselSuggestionViewProperties.ITEM_DECORATION,
                                new DynamicSpacingRecyclerViewItemDecoration(
                                        mInitialSpacing,
                                        mElementSpacing / 2,
                                        mCarouselItemViewWidth))
                        .build();

        return model;
    }

    @Override
    public int getCarouselItemViewHeight() {
        return mCarouselItemViewHeight;
    }

    @Override
    public void populateModel(
            AutocompleteInput input, AutocompleteMatch match, PropertyModel model, int matchIndex) {
        super.populateModel(input, match, model, matchIndex);

        List<ListItem> tileList = model.get(BaseCarouselSuggestionViewProperties.TILES);
        String title =
                TextUtils.isEmpty(match.getDescription())
                        ? match.getUrl().getHost()
                        : match.getDescription();
        int tileIndex = tileList.size();

        var tileModel =
                createTile(
                        title,
                        match.getUrl(),
                        match.isSearchSuggestion(),
                        v -> {
                            OmniboxMetrics.recordSuggestTileTypeUsed(
                                    tileIndex, match.isSearchSuggestion());
                            mSuggestionHost.onSuggestionClicked(match, matchIndex, match.getUrl());
                        },
                        v -> {
                            mSuggestionHost.confirmDeleteMatch(match, title);
                            return true;
                        });

        tileList.add(
                new ListItem(BaseCarouselSuggestionItemViewBuilder.ViewType.TILE_VIEW, tileModel));
    }

    private PropertyModel createTile(
            String title,
            GURL url,
            boolean isSearch,
            View.OnClickListener onClick,
            View.OnLongClickListener onLongClick) {
        String contentDescription;
        Drawable decoration;

        if (isSearch) {
            decoration =
                    OmniboxResourceProvider.getDrawable(
                            mContext, R.drawable.ic_suggestion_magnifier);
            contentDescription =
                    OmniboxResourceProvider.getString(
                            mContext,
                            R.string.accessibility_omnibox_most_visited_tile_search,
                            title);
        } else {
            decoration = OmniboxResourceProvider.getDrawable(mContext, R.drawable.ic_globe_24dp);
            contentDescription =
                    OmniboxResourceProvider.getString(
                            mContext,
                            R.string.accessibility_omnibox_most_visited_tile_navigate,
                            title,
                            url.getHost());
        }

        var model =
                new PropertyModel.Builder(MostVisitedTileViewProperties.ALL_KEYS)
                        .with(TileViewProperties.TITLE, title)
                        .with(TileViewProperties.TITLE_LINES, 1)
                        .with(
                                TileViewProperties.ON_FOCUS_VIA_SELECTION,
                                () -> mSuggestionHost.setOmniboxEditingText(url.getSpec()))
                        .with(TileViewProperties.ON_CLICK, onClick)
                        .with(TileViewProperties.ON_LONG_CLICK, onLongClick)
                        .with(
                                TileViewProperties.ICON_TINT,
                                ChromeColors.getSecondaryIconTint(
                                        mContext, /* forceLightIconTint= */ false))
                        .with(TileViewProperties.CONTENT_DESCRIPTION, contentDescription)
                        .with(TileViewProperties.ICON, decoration)
                        .with(
                                TileViewProperties.SMALL_ICON_ROUNDING_RADIUS,
                                mContext.getResources()
                                        .getDimensionPixelSize(
                                                R.dimen.omnibox_small_icon_rounding_radius))
                        .build();

        // Fetch site favicon for MV tiles.
        if (!isSearch) {
            if (mImageSupplier != null) {
                mImageSupplier.fetchFavicon(
                        url,
                        icon -> {
                            if (icon == null) {
                                mImageSupplier.generateFavicon(
                                        url,
                                        fallback -> {
                                            if (fallback == null) return;
                                            model.set(
                                                    TileViewProperties.ICON,
                                                    new BitmapDrawable(
                                                            mContext.getResources(), fallback));
                                            model.set(TileViewProperties.ICON_TINT, null);
                                        });
                                return;
                            }
                            model.set(
                                    TileViewProperties.ICON,
                                    new BitmapDrawable(mContext.getResources(), icon));
                            model.set(TileViewProperties.ICON_TINT, null);
                        });
            }
        }

        return model;
    }
}
