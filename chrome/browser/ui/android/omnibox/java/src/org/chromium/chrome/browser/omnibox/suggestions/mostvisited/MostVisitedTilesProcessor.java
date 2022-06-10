// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.mostvisited;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.carousel.BaseCarouselSuggestionItemViewBuilder;
import org.chromium.chrome.browser.omnibox.suggestions.carousel.BaseCarouselSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.carousel.BaseCarouselSuggestionViewProperties;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.browser_ui.widget.TintedDrawable;
import org.chromium.components.browser_ui.widget.tile.TileViewProperties;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.omnibox.AutocompleteMatch;
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
    private final @NonNull Supplier<LargeIconBridge> mIconBridgeSupplier;
    private final int mMinCarouselItemViewHeight;
    private final int mDesiredFaviconWidthPx;
    private @NonNull RoundedIconGenerator mIconGenerator;

    /**
     * Constructor.
     *
     * @param context An Android context.
     * @param host SuggestionHost receiving notifications about user actions.
     * @param iconBridgeSupplier Supplier of the LargeIconBridge used to fetch site favicons.
     */
    public MostVisitedTilesProcessor(@NonNull Context context, @NonNull SuggestionHost host,
            @NonNull Supplier<LargeIconBridge> iconBridgeSupplier) {
        super(context);
        mContext = context;
        mSuggestionHost = host;
        mIconBridgeSupplier = iconBridgeSupplier;
        mMinCarouselItemViewHeight =
                mContext.getResources().getDimensionPixelSize(R.dimen.tile_view_min_height);
        mDesiredFaviconWidthPx = mContext.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_favicon_size);

        int fallbackIconSize =
                mContext.getResources().getDimensionPixelSize(R.dimen.tile_view_icon_size);
        int fallbackIconColor = mContext.getColor(R.color.default_favicon_background_color);
        int fallbackIconTextSize =
                mContext.getResources().getDimensionPixelSize(R.dimen.tile_view_icon_text_size);
        mIconGenerator = new RoundedIconGenerator(fallbackIconSize, fallbackIconSize,
                fallbackIconSize / 2, fallbackIconColor, fallbackIconTextSize);
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
    public void populateModel(AutocompleteMatch suggestion, PropertyModel model, int matchIndex) {
        final List<AutocompleteMatch.SuggestTile> tiles = suggestion.getSuggestTiles();
        final int tilesCount = tiles.size();
        final List<ListItem> tileList = new ArrayList<>(tilesCount);
        final LargeIconBridge iconBridge = mIconBridgeSupplier.get();

        for (int elementIndex = 0; elementIndex < tilesCount; elementIndex++) {
            final PropertyModel tileModel = new PropertyModel(TileViewProperties.ALL_KEYS);
            final String title = tiles.get(elementIndex).title;
            final GURL url = tiles.get(elementIndex).url;
            tileModel.set(TileViewProperties.TITLE, title);
            tileModel.set(TileViewProperties.TITLE_LINES, 1);
            tileModel.set(TileViewProperties.ON_FOCUS_VIA_SELECTION,
                    () -> mSuggestionHost.setOmniboxEditingText(url.getSpec()));
            tileModel.set(TileViewProperties.ON_CLICK,
                    v -> mSuggestionHost.onSuggestionClicked(suggestion, matchIndex, url));

            final int elementIndexForDeletion = elementIndex;
            tileModel.set(TileViewProperties.ON_LONG_CLICK, v -> {
                mSuggestionHost.onDeleteMatchElement(
                        suggestion, title, matchIndex, elementIndexForDeletion);
                return true;
            });

            if (tiles.get(elementIndex).isSearch) {
                Drawable drawable = TintedDrawable.constructTintedDrawable(
                        mContext, R.drawable.ic_suggestion_magnifier);
                // Note: we should never show most visited tiles in incognito mode. Catch this early
                // if we ever do.
                assert model.get(SuggestionCommonProperties.COLOR_SCHEME)
                        != BrandedColorScheme.INCOGNITO;
                drawable.setTintList(
                        ChromeColors.getSecondaryIconTint(mContext, /* isIncognito= */ false));
                tileModel.set(TileViewProperties.ICON, drawable);
                tileModel.set(TileViewProperties.CONTENT_DESCRIPTION,
                        mContext.getString(
                                R.string.accessibility_omnibox_most_visited_tile_search, title));
            } else {
                tileModel.set(TileViewProperties.CONTENT_DESCRIPTION,
                        mContext.getString(
                                R.string.accessibility_omnibox_most_visited_tile_navigate, title,
                                url.getHost()));

                if (iconBridge != null) {
                    // TODO(https://crbug.com/1335507): Cache the generated bitmaps.
                    // This is not needed for the experimentation purposes (this block is currently
                    // used very infrequently - only to offer zero-prefix URL suggestions on URL
                    // visit). This must be addressed before MV Carousel can be offered in more
                    // contexts. Consider unifying cache with LargeIconBridge if possible.
                    Bitmap fallbackIcon = mIconGenerator.generateIconForUrl(url);
                    tileModel.set(TileViewProperties.ICON, new BitmapDrawable(fallbackIcon));

                    iconBridge.getLargeIconForUrl(tiles.get(elementIndex).url,
                            mDesiredFaviconWidthPx,
                            (Bitmap icon, int fallbackColor, boolean isFallbackColorDefault,
                                    int iconType) -> {
                                if (icon == null) return;
                                setIcon(tileModel, icon);
                            });
                }
            }

            tileList.add(new ListItem(
                    BaseCarouselSuggestionItemViewBuilder.ViewType.TILE_VIEW, tileModel));
        }

        model.set(BaseCarouselSuggestionViewProperties.TILES, tileList);
        model.set(BaseCarouselSuggestionViewProperties.SHOW_TITLE, false);
    }

    /**
     * Sets the large icon to the supplied tile's model.
     *
     * @param tileModel The model for the specific tile view to be modified.
     * @param icon The icon to apply.
     */
    private void setIcon(@NonNull PropertyModel tileModel, @NonNull Bitmap icon) {
        tileModel.set(TileViewProperties.SMALL_ICON_ROUNDING_RADIUS,
                mContext.getResources().getDimensionPixelSize(
                        R.dimen.omnibox_carousel_icon_rounding_radius));
        tileModel.set(TileViewProperties.ICON, new BitmapDrawable(icon));
    }

    /**
     * Overrides RoundedIconGenerator for testing.
     * @param RoundedIconGenerator Generator to use.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void setRoundedIconGeneratorForTesting(@NonNull RoundedIconGenerator generator) {
        mIconGenerator = generator;
    }
}
