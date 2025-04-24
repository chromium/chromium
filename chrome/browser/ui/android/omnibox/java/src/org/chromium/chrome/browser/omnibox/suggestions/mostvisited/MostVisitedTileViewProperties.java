// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.mostvisited;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.components.browser_ui.widget.tile.TileViewProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** List of properties recognized by Omnibox MostVisitedTiles. */
@NullMarked
public interface MostVisitedTileViewProperties {
    public static final PropertyKey[] ALL_KEYS =
            PropertyModel.concatKeys(
                    TileViewProperties.ALL_KEYS, SuggestionCommonProperties.ALL_KEYS);
}
