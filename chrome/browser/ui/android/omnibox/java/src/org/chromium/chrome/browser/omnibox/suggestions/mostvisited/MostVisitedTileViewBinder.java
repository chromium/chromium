// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.mostvisited;

import android.graphics.drawable.Drawable;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.widget.tile.TileView;
import org.chromium.components.browser_ui.widget.tile.TileViewBinder;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** ViewBinder for a single Most Visited Tile. */
@NullMarked
public class MostVisitedTileViewBinder {
    /**
     * @see PropertyModelChangeProcessor.ViewBinder#bind(Object, Object, Object)
     */
    public static void bind(PropertyModel model, TileView view, PropertyKey propertyKey) {
        if (SuggestionCommonProperties.COLOR_SCHEME == propertyKey) {
            updateColorScheme(model, view);
        }
        TileViewBinder.bind(model, view, propertyKey);
    }

    private static void updateColorScheme(PropertyModel model, TileView view) {
        var context = view.getContext();
        @BrandedColorScheme int scheme = model.get(SuggestionCommonProperties.COLOR_SCHEME);
        Drawable background =
                OmniboxResourceProvider.getStatefulSuggestionBackground(
                        context,
                        OmniboxResourceProvider.getSuggestionsDropdownBackgroundColor(
                                context, scheme),
                        scheme);
        view.setBackground(background);
    }
}
