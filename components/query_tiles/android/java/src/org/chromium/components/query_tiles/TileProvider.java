// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.query_tiles;

import org.chromium.base.Callback;

import java.util.List;

/**
 * Java interface for interacting with the native query tile service. Responsible for initializing
 * and fetching data fo be shown on the UI.
 */
public interface TileProvider {
    /**
     * Called to retrieve all the tiles.
     * @param callback The {@link Callback} to be notified on completion. Returns an empty list if
     *         no tiles are found.
     */
    void getQueryTiles(Callback<List<QueryTile>> callback);

    /**
     * Called when a tile is clicked.
     * @param tildId ID of the tile.
     */
    void onTileClicked(String tileId);
}
