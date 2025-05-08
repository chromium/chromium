// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.carousel;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.graphics.Color;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.ViewOutlineProvider;

import androidx.annotation.ColorInt;
import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.components.browser_ui.widget.RoundedCornerOutlineProvider;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/** Binder for the Carousel suggestions. */
@NullMarked
public interface BaseCarouselSuggestionViewBinder {
    /**
     * @see PropertyModelChangeProcessor.ViewBinder#bind(Object, Object, Object)
     */
    public static void bind(PropertyModel model, BaseCarouselSuggestionView view, PropertyKey key) {

        if (key == BaseCarouselSuggestionViewProperties.TILES) {
            var items = model.get(BaseCarouselSuggestionViewProperties.TILES);
            var adapter = assumeNonNull((SimpleRecyclerViewAdapter) view.getAdapter());
            if (items != null) {
                adapter.getModelList().set(items);
            } else {
                adapter.getModelList().clear();
            }
            view.resetSelection();
            propagateCommonProperties(adapter.getModelList(), model);
        } else if (key == SuggestionCommonProperties.COLOR_SCHEME) {
            // Propagate color scheme to all tiles.
            var adapter = assumeNonNull((SimpleRecyclerViewAdapter) view.getAdapter());
            propagateCommonProperties(adapter.getModelList(), model);
        } else if (key == BaseCarouselSuggestionViewProperties.ITEM_DECORATION) {
            view.setItemDecoration(model.get(BaseCarouselSuggestionViewProperties.ITEM_DECORATION));
        } else if (key == BaseCarouselSuggestionViewProperties.CONTENT_DESCRIPTION) {
            view.setContentDescription(
                    model.get(BaseCarouselSuggestionViewProperties.CONTENT_DESCRIPTION));
        } else if (key == BaseCarouselSuggestionViewProperties.TOP_PADDING
                || key == BaseCarouselSuggestionViewProperties.BOTTOM_PADDING) {
            int top = model.get(BaseCarouselSuggestionViewProperties.TOP_PADDING);
            int bottom = model.get(BaseCarouselSuggestionViewProperties.BOTTOM_PADDING);
            view.setPaddingRelative(0, top, 0, bottom);
        } else if (key == BaseCarouselSuggestionViewProperties.APPLY_BACKGROUND) {
            boolean useBackground =
                    model.get(BaseCarouselSuggestionViewProperties.APPLY_BACKGROUND);

            // Default values to be used if background is disabled.
            @ColorInt int bgColor = Color.TRANSPARENT;
            @Px int horizontalMargin = 0;
            ViewOutlineProvider outline = null;

            // Specific values to apply if background is enabled.
            if (useBackground) {
                // Note: this assumes carousel is not showing in the incognito mode.
                bgColor =
                        OmniboxResourceProvider.getStandardSuggestionBackgroundColor(
                                view.getContext(),
                                model.get(SuggestionCommonProperties.COLOR_SCHEME));
                horizontalMargin = OmniboxResourceProvider.getSideSpacing(view.getContext());
                outline =
                        new RoundedCornerOutlineProvider(
                                view.getContext()
                                        .getResources()
                                        .getDimensionPixelSize(
                                                R.dimen.omnibox_suggestion_bg_round_corner_radius));
            }

            // Apply values.
            view.setBackgroundColor(bgColor);
            var layoutParams = view.getLayoutParams();
            if (layoutParams instanceof MarginLayoutParams) {
                ((MarginLayoutParams) layoutParams)
                        .setMargins(horizontalMargin, 0, horizontalMargin, 0);
                view.setLayoutParams(layoutParams);
            }

            view.setOutlineProvider(outline);
            view.setClipToOutline(outline != null);
        }
    }

    private static void propagateCommonProperties(ModelList list, PropertyModel model) {
        for (int i = 0; i < list.size(); i++) {
            PropertyModel tileModel = list.get(i).model;

            tileModel.set(
                    SuggestionCommonProperties.COLOR_SCHEME,
                    model.get(SuggestionCommonProperties.COLOR_SCHEME));
        }
    }
}
