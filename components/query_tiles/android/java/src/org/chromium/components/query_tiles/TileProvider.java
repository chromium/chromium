// Copyright 2020 The Chromium Authors
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
     * Called to retrieve all the subtiles for a parent tile with the given Id. If the Id is null,
     * all top level tiles will be returned.
     * @param tileId ID of the parent tile. If null, all top level tiles will be returned.
     * @param callback The {@link Callback} to be notified on completion. Returns an empty list if
     *         no tiles are found.
     */
    void getQueryTiles(String tileId, Callback<List<QueryTile>> callback);

    /**
     * Called when a tile is clicked.
     * @param tildId ID of the tile.
     */
    void onTileClicked(String tileId);
}
