// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.carousel;

import android.content.res.Resources;
import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties.FormFactor;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.util.Collection;

/**
 * Binder for the Carousel suggestions.
 */
public final class BaseCarouselSuggestionViewBinder {
    /** @see PropertyModelChangeProcessor.ViewBinder#bind(Object, Object, Object) */
    public static void bind(PropertyModel model, BaseCarouselSuggestionView view, PropertyKey key) {
        if (key == BaseCarouselSuggestionViewProperties.TILES) {
            final Collection<ListItem> items =
                    model.get(BaseCarouselSuggestionViewProperties.TILES);
            final SimpleRecyclerViewAdapter adapter = view.getAdapter();
            if (items != null) {
                adapter.getModelList().set(items);
            } else {
                adapter.getModelList().clear();
            }
        } else if (key == BaseCarouselSuggestionViewProperties.TITLE) {
            view.getHeaderTextView().setText(model.get(BaseCarouselSuggestionViewProperties.TITLE));
        } else if (key == BaseCarouselSuggestionViewProperties.SHOW_TITLE) {
            final boolean showTitle = model.get(BaseCarouselSuggestionViewProperties.SHOW_TITLE);
            final View headerView = view.getHeaderView();
            final int verticalPad = view.getResources().getDimensionPixelSize(
                    R.dimen.omnibox_carousel_suggestion_padding);
            if (showTitle) {
                headerView.setVisibility(View.VISIBLE);
                view.setPaddingRelative(0, 0, 0, verticalPad);
            } else {
                headerView.setVisibility(View.GONE);
                view.setPaddingRelative(0, verticalPad, 0, verticalPad);
            }
        } else if (key == SuggestionCommonProperties.DEVICE_FORM_FACTOR) {
            view.setItemSpacingPx(getItemSpacingPx(
                    model.get(SuggestionCommonProperties.DEVICE_FORM_FACTOR), view.getResources()));
        }
    }

    /**
     * Calculate the margin between tiles based on screen size.
     *
     * @param formFactor the form factor of the device, from which we differentiate between PHONE
     *         and TABLET.
     * @param resources Android resources object, used to read the dimension.
     * @return The requested item spacing, expressed in Pixels.
     */
    static int getItemSpacingPx(@FormFactor int formFactor, @NonNull Resources resources) {
        int tileViewPortraitEdgePadding =
                resources.getDimensionPixelSize(R.dimen.tile_view_padding_edge_portrait);
        switch (formFactor) {
            case FormFactor.PHONE:
                int screenWidth = resources.getDisplayMetrics().widthPixels;
                int tileViewWidth = resources.getDimensionPixelOffset(R.dimen.tile_view_width);
                return Integer.max(-resources.getDimensionPixelOffset(R.dimen.tile_view_padding),
                        (int) ((screenWidth - tileViewPortraitEdgePadding - tileViewWidth * 4.7)
                                / 4));
            case FormFactor.TABLET:
                return tileViewPortraitEdgePadding;
            default:
                assert false : "Unknown device type";
                return 0;
        }
    }
}
