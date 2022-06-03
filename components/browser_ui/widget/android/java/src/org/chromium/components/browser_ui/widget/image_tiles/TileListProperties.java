// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.image_tiles;

import org.chromium.base.Callback;
import org.chromium.components.browser_ui.widget.image_tiles.ImageTileCoordinator.TileVisualsProvider;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * The properties required to build a {@link ImageTile} which contain two types of properties:
 * (1) A set of properties that act directly on the list view itself. (2) A set of
 * properties that are effectively shared across all list items like callbacks.
 */
interface TileListProperties {
    /** The callback to run when a {@link ImageTile} is clicked on the UI. */
    WritableObjectPropertyKey<Callback<ImageTile>> CLICK_CALLBACK =
            new WritableObjectPropertyKey<>();

    /** The provider to retrieve expensive visuals for a {@link ImageTile}. */
    WritableObjectPropertyKey<TileVisualsProvider> VISUALS_CALLBACK =
            new WritableObjectPropertyKey<>();

    PropertyKey[] ALL_KEYS = new PropertyKey[] {CLICK_CALLBACK, VISUALS_CALLBACK};
}