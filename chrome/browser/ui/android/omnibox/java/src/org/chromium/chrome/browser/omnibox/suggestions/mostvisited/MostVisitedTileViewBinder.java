// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.mostvisited;

import android.graphics.drawable.Drawable;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.components.browser_ui.widget.tile.TileView;
import org.chromium.components.browser_ui.widget.tile.TileViewBinder;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** ViewBinder for a single Most Visited Tile. */
@NullMarked
public class MostVisitedTileViewBinder
        implements PropertyModelChangeProcessor.ViewBinder<PropertyModel, TileView, PropertyKey> {
    private final OmniboxResourceProvider mResourceProvider;

    public MostVisitedTileViewBinder(OmniboxResourceProvider resourceProvider) {
        mResourceProvider = resourceProvider;
    }

    /**
     * @see PropertyModelChangeProcessor.ViewBinder#bind(Object, Object, Object)
     */
    @Override
    public void bind(PropertyModel model, TileView view, PropertyKey propertyKey) {
        if (SuggestionCommonProperties.COLOR_SCHEME == propertyKey) {
            updateColorScheme(view);
        }
        TileViewBinder.bind(model, view, propertyKey);
    }

    private void updateColorScheme(TileView view) {
        Drawable background =
                mResourceProvider.getStatefulSuggestionBackground(
                        mResourceProvider.getSuggestionsDropdownBackgroundColor());
        view.setBackground(background);
    }
}
