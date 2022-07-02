// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.carousel;

import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.widget.tile.TileView;
import org.chromium.components.browser_ui.widget.tile.TileViewBinder;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * ViewBuilder for the Carousel suggestion.
 * Its sole responsibility is to inflate appropriate view layouts for supplied view type.
 */
public class BaseCarouselSuggestionItemViewBuilder {
    /**
     * ViewType defines a list of Views that are understood by the Carousel.
     * Views below can be used by any instance of the carousel, guaranteeing that each instance
     * will look like every other.
     */
    @IntDef({ViewType.TILE_VIEW})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ViewType {
        /** Carousel item is a TileView instance. */
        public int TILE_VIEW = 0;
    }

    /**
     * Create standard Carousel Suggestion View capable of hosting any of the ViewTypes.
     *
     * @param parent ViewGroup that will host the Carousel view.
     * @return BaseCarouselSuggestionView.
     */
    public static BaseCarouselSuggestionView createView(ViewGroup parent) {
        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(new ModelList());
        adapter.registerType(ViewType.TILE_VIEW,
                BaseCarouselSuggestionItemViewBuilder::createTileView, TileViewBinder::bind);
        return new BaseCarouselSuggestionView(parent.getContext(), adapter);
    }

    /**
     * Create a standard TileView element.
     *
     * @param parent ViewGroup that will host the Tile.
     * @return A TileView element for the individual URL suggestion.
     */
    private static TileView createTileView(ViewGroup parent) {
        TileView tile = (TileView) LayoutInflater.from(parent.getContext())
                                .inflate(R.layout.suggestions_tile_view, parent, false);
        tile.setClickable(true);

        Drawable background =
                OmniboxResourceProvider.resolveAttributeToDrawable(parent.getContext(),
                        BrandedColorScheme.LIGHT_BRANDED_THEME, R.attr.selectableItemBackground);
        tile.setBackgroundDrawable(background);
        return tile;
    }
}
