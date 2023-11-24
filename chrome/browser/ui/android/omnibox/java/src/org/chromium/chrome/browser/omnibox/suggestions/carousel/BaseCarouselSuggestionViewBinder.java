// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.carousel;

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
        } else if (key == BaseCarouselSuggestionViewProperties.ITEM_WIDTH) {
            view.getItemDecoration()
                    .setItemWidth(model.get(BaseCarouselSuggestionViewProperties.ITEM_WIDTH));
        } else if (key == BaseCarouselSuggestionViewProperties.CONTENT_DESCRIPTION) {
            view.setContentDescription(
                    model.get(BaseCarouselSuggestionViewProperties.CONTENT_DESCRIPTION));
        } else if (key == BaseCarouselSuggestionViewProperties.HORIZONTAL_FADE) {
            view.setHorizontalFadingEdgeEnabled(
                    model.get(BaseCarouselSuggestionViewProperties.HORIZONTAL_FADE));
        }
    }
}
