// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.image_tiles;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.components.browser_ui.widget.image_tiles.ImageTileCoordinator.TileVisualsProvider;

/**
 * Factory to create an {@link ImageTileCoordinator} instance.
 * The {@link ImageTileCoordinator} is a generic widget that can display a list of images inside a
 * carousel. For using this widget,
 *   - Create a {@link ImageTileCoordinator}.
 *   - Call {@link ImageTileCoordinator#setTiles(List)} with a list of {@link ImageTile}s to be
 *     shown.
 *   - Implement {@link TileVisualsProvider} to provide thumbnails for the images.
 *   - Define click handlers to be invoked when the tiles are clicked.
 */
public class ImageTileCoordinatorFactory {
    /**
     * Creates a {@link ImageTileCoordinator}.
     * @param context The context associated with the current activity.
     * @param tileClickCallback A callback to be invoked when a tile is clicked.
     * @return A {@link ImageTileCoordinator}.
     */
    public static ImageTileCoordinator create(
            Context context,
            TileConfig config,
            Callback<ImageTile> tileClickCallback,
            TileVisualsProvider tileVisualsProvider) {
        return new TileCoordinatorImpl(context, config, tileClickCallback, tileVisualsProvider);
    }
}
