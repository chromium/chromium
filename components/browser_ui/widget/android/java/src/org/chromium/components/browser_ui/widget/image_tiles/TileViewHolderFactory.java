// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.image_tiles;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.supplier.Supplier;
import org.chromium.components.browser_ui.widget.R;
import org.chromium.components.browser_ui.widget.image_tiles.TileSizeSupplier.TileSize;
import org.chromium.ui.modelutil.RecyclerViewAdapter;

/**
 * A factory class to create view holders for the tiles.
 */
class TileViewHolderFactory implements RecyclerViewAdapter.ViewHolderFactory<TileViewHolder> {
    private final Supplier<TileSize> mTileSizeSupplier;

    /**
     * Constructor.
     * @param tileSizeSupplier The {@link TileSize} to provide width and height for the view.
     */
    public TileViewHolderFactory(Supplier<TileSize> tileSizeSupplier) {
        mTileSizeSupplier = tileSizeSupplier;
    }

    @Override
    public TileViewHolder createViewHolder(ViewGroup parent, int viewType) {
        View view = LayoutInflater.from(parent.getContext())
                            .inflate(R.layout.query_tile_view, parent, false);
        view.getLayoutParams().width = mTileSizeSupplier.get().width;
        view.getLayoutParams().height = mTileSizeSupplier.get().width;
        return new TileViewHolder(view);
    }
}
