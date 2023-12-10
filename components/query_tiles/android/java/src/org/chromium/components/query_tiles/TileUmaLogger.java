// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.query_tiles;

import android.text.TextUtils;

import org.chromium.base.metrics.RecordHistogram;

import java.util.List;

/** Helper class to log metrics for various user actions associated with query tiles. */
public class TileUmaLogger {
    private final String mHistogramPrefix;
    private List<QueryTile> mTopLevelTiles;

    public TileUmaLogger(String histogramPrefix) {
        mHistogramPrefix = histogramPrefix;
    }

    /**
     * Called when a new set of tiles are loaded.
     * @param tiles The tiles loaded in the UI.
     */
    public void recordTilesLoaded(List<QueryTile> tiles) {
        if (tiles == null || tiles.isEmpty()) return;

        // We only care about the first time tiles are loaded. The subsequent calls to this function
        // are ignored as they might be called for sub-level tiles and we already have the tree
        // information.
        if (mTopLevelTiles != null) return;
        mTopLevelTiles = tiles;

        RecordHistogram.recordCount1MHistogram(
                "Search." + mHistogramPrefix + ".TileCount", mTopLevelTiles.size());
    }

    public void recordTileClicked(QueryTile tile) {
        assert tile != null;

        boolean isTopLevel = isTopLevelTile(tile.id);
        RecordHistogram.recordBooleanHistogram(
                "Search." + mHistogramPrefix + ".Tile.Clicked.IsTopLevel", isTopLevel);

        int tileUmaId = getTileUmaId(tile.id);
        RecordHistogram.recordSparseHistogram(
                "Search." + mHistogramPrefix + ".Tile.Clicked", tileUmaId);
    }

    private boolean isTopLevelTile(String id) {
        for (QueryTile tile : mTopLevelTiles) {
            if (tile.id.equals(id)) return true;
        }
        return false;
    }

    /**
     * Returns a UMA ID for the tile which will be used to identify the tile for metrics purposes.
     * The UMA ID is generated from the position of the tile in the query tile tree.
     * Top level tiles will be numbered 0,1,2... while the next level tile start
     * from 100 (100, 101, ... etc), 200 (200, 201, ... etc), and so on.
     */
    private int getTileUmaId(String tileId) {
        assert !TextUtils.isEmpty(tileId);

        for (int i = 0; i < mTopLevelTiles.size(); i++) {
            int found = search(mTopLevelTiles.get(i), tileId, i);
            if (found != -1) return found;
        }

        return -1;
    }

    private int search(QueryTile tile, String id, int startPosition) {
        if (id.equals(tile.id)) return startPosition;
        startPosition = (startPosition + 1) * 100;
        for (int i = 0; i < tile.children.size(); i++) {
            QueryTile child = tile.children.get(i);
            int found = search(child, id, startPosition + i);
            if (found != -1) return found;
        }
        return -1;
    }
}
