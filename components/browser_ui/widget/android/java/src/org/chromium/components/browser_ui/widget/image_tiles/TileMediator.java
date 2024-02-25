// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.image_tiles;

import android.graphics.Bitmap;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.components.browser_ui.widget.image_tiles.ImageTileCoordinator.TileVisualsProvider;

import java.util.List;

/**
 * The mediator for the tiles UI, responsible for reacting to events from outside world, and
 * interacting with other coordinators.
 */
class TileMediator {
    private final TileListModel mModel;
    private final TileConfig mConfig;
    private final Callback<ImageTile> mTileClickCallback;
    private final TileVisualsProvider mTileVisualsProvider;

    /** Constructor. */
    public TileMediator(
            TileConfig config,
            TileListModel model,
            Callback<ImageTile> tileClickCallback,
            TileVisualsProvider visualsProvider) {
        mModel = model;
        mConfig = config;
        mTileClickCallback = tileClickCallback;
        mTileVisualsProvider = visualsProvider;

        mModel.getProperties().set(TileListProperties.CLICK_CALLBACK, this::onTileClicked);
        mModel.getProperties().set(TileListProperties.VISUALS_CALLBACK, this::getVisuals);
    }

    private void onTileClicked(ImageTile tile) {
        RecordUserAction.record("Search." + mConfig.umaPrefix + ".Tile.Clicked");
        mTileClickCallback.onResult(tile);
    }

    private void getVisuals(ImageTile tile, Callback<List<Bitmap>> callback) {
        final long startTime = System.currentTimeMillis();
        mTileVisualsProvider.getVisuals(
                tile,
                visuals -> {
                    boolean visualsAvailable = visuals != null && !visuals.isEmpty();
                    recordTileVisuals(visualsAvailable, System.currentTimeMillis() - startTime);
                    callback.onResult(visuals);
                });
    }

    private void recordTileVisuals(boolean visualsAvailable, long durationMs) {
        RecordHistogram.recordBooleanHistogram(
                "Search." + mConfig.umaPrefix + ".Bitmap.Available", visualsAvailable);

        String fetchDurationHistogramName =
                "Search."
                        + mConfig.umaPrefix
                        + (visualsAvailable ? ".Bitmap" : ".NoBitmap")
                        + ".FetchDuration";
        RecordHistogram.recordTimesHistogram(fetchDurationHistogramName, durationMs);
    }
}
