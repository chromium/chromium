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
import androidx.annotation.Px;

import org.chromium.chrome.browser.omnibox.OmniboxMetrics;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxImageSupplier;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.base.DynamicSpacingRecyclerViewItemDecoration;
import org.chromium.chrome.browser.omnibox.suggestions.carousel.BaseCarouselSuggestionItemViewBuilder;
import org.chromium.chrome.browser.omnibox.suggestions.carousel.BaseCarouselSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.carousel.BaseCarouselSuggestionViewProperties;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.tile.TileViewProperties;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;
import java.util.Optional;

/** SuggestionProcessor for Most Visited URL tiles. */
public class MostVisitedTilesProcessor extends BaseCarouselSuggestionProcessor {
    private final @NonNull SuggestionHost mSuggestionHost;
    private final @NonNull Optional<OmniboxImageSupplier> mImageSupplier;
    private final @Px int mCarouselItemViewWidth;
    private final @Px int mCarouselItemViewHeight;
    private final @Px int mInitialSpacing;
    private final @Px int mElementSpacing;

    /**
     * Constructor.
     *
     * @param context An Android context.
     * @param host SuggestionHost receiving notifications about user actions.
     * @param imageSupplier Class retrieving favicons for the MV Tiles.
     */
    public MostVisitedTilesProcessor(
            @NonNull Context context,
            @NonNull SuggestionHost host,
            @NonNull Optional<OmniboxImageSupplier> imageSupplier) {
        super(context);
        mSuggestionHost = host;
        mImageSupplier = imageSupplier;
        mCarouselItemViewWidth =
                mContext.getResources().getDimensionPixelSize(R.dimen.tile_view_width);
        mCarouselItemViewHeight =
                mContext.getResources().getDimensionPixelSize(R.dimen.tile_view_min_height);

        mInitialSpacing =
                OmniboxResourceProvider.getHeaderStartPadding(context)
                        - context.getResources().getDimensionPixelSize(R.dimen.tile_view_padding);
        mElementSpacing =
                context.getResources()
                        .getDimensionPixelSize(
                                R.dimen.omnibox_carousel_suggestion_minimum_item_spacing);
    }

    @Override
    public boolean doesProcessSuggestion(@NonNull AutocompleteMatch match, int matchIndex) {
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
    public @NonNull PropertyModel createModel() {
        @SuppressWarnings("null")
        @NonNull
        PropertyModel model =
                new PropertyModel.Builder(BaseCarouselSuggestionViewProperties.ALL_KEYS)
                        .with(BaseCarouselSuggestionViewProperties.TILES, new ArrayList<>())
                        .with(
                                BaseCarouselSuggestionViewProperties.CONTENT_DESCRIPTION,
                                mContext.getResources()
                                        .getString(
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
    public void populateModel(AutocompleteMatch match, PropertyModel model, int matchIndex) {
        super.populateModel(match, model, matchIndex);

        List<ListItem> tileList = model.get(BaseCarouselSuggestionViewProperties.TILES);

        @SuppressWarnings("null")
        @NonNull
        String title =
                TextUtils.isEmpty(match.getDisplayText())
                        ? match.getUrl().getHost()
                        : match.getDisplayText();
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
                            mSuggestionHost.onDeleteMatch(match, title);
                            return true;
                        });

        tileList.add(
                new ListItem(BaseCarouselSuggestionItemViewBuilder.ViewType.TILE_VIEW, tileModel));
    }

    private PropertyModel createTile(
            @NonNull String title,
            @NonNull GURL url,
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
                new PropertyModel.Builder(TileViewProperties.ALL_KEYS)
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
                                        mContext, /* isIncognito= */ false))
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
            mImageSupplier.ifPresent(
                    s ->
                            s.fetchFavicon(
                                    url,
                                    icon -> {
                                        if (icon == null) {
                                            s.generateFavicon(
                                                    url,
                                                    fallback -> {
                                                        if (fallback == null) return;
                                                        model.set(
                                                                TileViewProperties.ICON,
                                                                new BitmapDrawable(
                                                                        mContext.getResources(),
                                                                        fallback));
                                                        model.set(
                                                                TileViewProperties.ICON_TINT, null);
                                                    });
                                            return;
                                        }
                                        model.set(
                                                TileViewProperties.ICON,
                                                new BitmapDrawable(mContext.getResources(), icon));
                                        model.set(TileViewProperties.ICON_TINT, null);
                                    }));
        }

        return model;
    }
}
