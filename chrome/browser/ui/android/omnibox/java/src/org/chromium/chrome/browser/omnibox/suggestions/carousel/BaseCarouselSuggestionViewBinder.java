// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.carousel;

import android.content.res.Configuration;
import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.omnibox.OmniboxFeatures;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties.FormFactor;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/** Binder for the Carousel suggestions. */
public interface BaseCarouselSuggestionViewBinder {
    /**
     * @see PropertyModelChangeProcessor.ViewBinder#bind(Object, Object, Object)
     */
    public static void bind(PropertyModel model, BaseCarouselSuggestionView view, PropertyKey key) {

        if (key == BaseCarouselSuggestionViewProperties.TILES) {
            var items = model.get(BaseCarouselSuggestionViewProperties.TILES);
            var adapter = (SimpleRecyclerViewAdapter) view.getAdapter();
            if (items != null) {
                adapter.getModelList().set(items);
            } else {
                adapter.getModelList().clear();
            }
        } else if (key == SuggestionCommonProperties.DEVICE_FORM_FACTOR
                || key == BaseCarouselSuggestionViewProperties.ITEM_WIDTH) {
            var context = view.getContext();
            // Adjust the initial offset of the MV Carousel to match the offset of the
            // suggestion header.
            int tileViewPadding =
                    view.getResources().getDimensionPixelSize(R.dimen.tile_view_padding);
            int initialSpacing =
                    OmniboxFeatures.shouldShowModernizeVisualUpdate(context)
                            ? OmniboxResourceProvider.getHeaderStartPadding(context)
                                    - tileViewPadding
                            : OmniboxResourceProvider.getSideSpacing(context);
            int itemSpacing =
                    getItemSpacingPx(
                            model.get(SuggestionCommonProperties.DEVICE_FORM_FACTOR),
                            model.get(BaseCarouselSuggestionViewProperties.ITEM_WIDTH),
                            initialSpacing,
                            view);
            view.getItemDecoration().setLeadInSpace(initialSpacing);
            view.getItemDecoration().setElementSpace(itemSpacing / 2);
        } else if (key == BaseCarouselSuggestionViewProperties.HORIZONTAL_FADE) {
            view.setHorizontalFadingEdgeEnabled(
                    model.get(BaseCarouselSuggestionViewProperties.HORIZONTAL_FADE));
        }
    }

    /**
     * Calculate the margin between tiles based on screen size.
     *
     * @param formFactor the form factor of the device, from which we differentiate between PHONE
     *     and TABLET
     * @param itemWidth the width of an individual element in the carousel
     * @param initialSpacing the space between the start edge of the suggestions dropdown and the
     *     first item on the carousel
     * @param view Android view object to evaluate and update
     * @return the requested item spacing, expressed in Pixels
     */
    static int getItemSpacingPx(
            @FormFactor int formFactor, int itemWidth, int initialSpacing, @NonNull View view) {
        var resources = view.getResources();

        // We defer to fixed spacing in landscape mode.
        if (resources.getConfiguration().orientation == Configuration.ORIENTATION_LANDSCAPE) {
            return resources.getDimensionPixelOffset(R.dimen.tile_view_padding_landscape);
        }

        // Otherwise, we enable dynamic spacing in portrait mode on phones and tablets where revamp
        // is enabled.
        boolean enableDynamicSizing =
                formFactor == FormFactor.PHONE
                        || OmniboxFeatures.shouldShowModernizeVisualUpdate(view.getContext());

        int baseSpacing = resources.getDimensionPixelSize(R.dimen.tile_view_padding_edge_portrait);
        if (enableDynamicSizing) {
            // Compute item spacing, guaranteeing exactly 50% exposure of one item
            // given the carousel width, item width, initial spacing, and base item spacing.
            // Resulting item spacing must be no smaller than base item spacing.
            //
            // Given a carousel entry:
            //   |__XXXX...XXXX...XXXX...XX|
            // where:
            // - '|' marks boundaries of the carousel,
            // - '_' is the initial spacing,
            // - 'X' is a carousel element, and
            // - '.' is the item space
            // computes the width of item space ('.').
            int carouselWidth = view.getMeasuredWidth();
            int adjustedCarouselWidth = carouselWidth - initialSpacing;
            int itemAndSpaceWidth = itemWidth + baseSpacing;
            int numberOfFullyVisibleItems = adjustedCarouselWidth / itemAndSpaceWidth;

            // We know the number of items that will be fully visible on screen.
            // Another item may be partially exposed.
            // Now we check how much of that item is visible; if it's less than 50% exposed, we
            // reduce number of fully exposed items to show, and increase padding.
            if ((adjustedCarouselWidth - numberOfFullyVisibleItems * itemAndSpaceWidth)
                    < 0.5 * itemWidth) {
                numberOfFullyVisibleItems--;
            }

            // If tiles are too large (i.e. larger than the screen width), just return default
            // padding. There's nothing we can do.
            if (numberOfFullyVisibleItems <= 0) {
                return baseSpacing;
            }

            int totalPaddingAreaSize =
                    adjustedCarouselWidth - (int) ((numberOfFullyVisibleItems + 0.5) * itemWidth);
            int itemSpacing = totalPaddingAreaSize / numberOfFullyVisibleItems;
            return itemSpacing;
        } else {
            return baseSpacing;
        }
    }
}
