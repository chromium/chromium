// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.image_tiles;

import android.graphics.Bitmap;
import android.view.View;

import org.chromium.base.Callback;

import java.util.List;

/** The top level coordinator for the image tiles UI. */
public interface ImageTileCoordinator {
    /** @return A {@link View} representing this coordinator. */
    View getView();

    /**
     * Sets a list of tiles to be displayed.
     * @param tiles The list of tiles to be displayed.
     */
    void setTiles(List<ImageTile> tiles);

    /** Refresh tile display. If tiles are scrolled, return them to their original position. */
    void refreshTiles();

    /** A helper interface to support retrieving {@link Bitmap}s asynchronously. */
    @FunctionalInterface
    interface TileVisualsProvider {
        /**
         * Called to get the visuals required for showing the tile. The result consists a list of
         * bitmaps, as the UI might use more than one bitmap to represent the tile.
         * @param tile The {@link ImageTile} to get the {@link Bitmap} for.
         * @param callback A {@link Callback} that will be notified on completion.
         */
        void getVisuals(ImageTile tile, Callback<List<Bitmap>> callback);
    }
}
