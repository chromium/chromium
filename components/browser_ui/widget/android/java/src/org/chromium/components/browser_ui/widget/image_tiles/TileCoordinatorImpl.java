// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.image_tiles;

import android.content.Context;
import android.view.View;

import org.chromium.base.Callback;

import java.util.ArrayList;
import java.util.List;

/** The top level coordinator for the tiles UI. */
class TileCoordinatorImpl implements ImageTileCoordinator {
    private final TileListModel mModel;
    private final TileListView mView;

    /** Constructor. */
    public TileCoordinatorImpl(
            Context context,
            TileConfig config,
            Callback<ImageTile> tileClickCallback,
            TileVisualsProvider visualsProvider) {
        mModel = new TileListModel();
        mView = new TileListView(context, config, mModel);
        new TileMediator(config, mModel, tileClickCallback, visualsProvider);
    }

    @Override
    public View getView() {
        return mView.getView();
    }

    @Override
    public void setTiles(List<ImageTile> tiles) {
        // Determine if the old set of tiles have changed. If yes, show animation.
        List<ImageTile> oldTiles = new ArrayList<>();
        for (int i = 0; i < mModel.size(); i++) {
            oldTiles.add(mModel.get(i));
        }
        boolean shouldAnimate = !oldTiles.isEmpty() && !tiles.isEmpty() && !oldTiles.equals(tiles);

        mModel.set(tiles);
        mView.scrollToBeginning();
        mView.showAnimation(shouldAnimate);
    }

    @Override
    public void refreshTiles() {
        mView.scrollToBeginning();
        mView.showAnimation(false);
    }
}
