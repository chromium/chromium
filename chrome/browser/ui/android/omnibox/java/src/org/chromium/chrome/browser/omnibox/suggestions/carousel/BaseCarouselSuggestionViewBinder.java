// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.carousel;

import android.content.Context;
import android.graphics.Color;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.ViewOutlineProvider;

import androidx.annotation.ColorInt;
import androidx.annotation.Px;

import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.widget.RoundedCornerOutlineProvider;
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
            view.resetSelection();
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
                bgColor = getSuggestionBackgroundColor(model, view.getContext());
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

    /**
     * Retrieve the background color to be applied to suggestion.
     *
     * @param model A property model to look up relevant properties.
     * @param ctx Context used to retrieve appropriate color value. @ColorInt value representing the
     *     color to be applied.
     */
    public static @ColorInt int getSuggestionBackgroundColor(PropertyModel model, Context ctx) {
        return model.get(SuggestionCommonProperties.COLOR_SCHEME) == BrandedColorScheme.INCOGNITO
                ? ctx.getColor(R.color.omnibox_suggestion_bg_incognito)
                : OmniboxResourceProvider.getStandardSuggestionBackgroundColor(ctx);
    }
}
