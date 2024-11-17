// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.image_tiles;

import android.content.Context;
import android.content.res.Resources;

import org.chromium.base.supplier.Supplier;
import org.chromium.components.browser_ui.widget.R;
import org.chromium.components.browser_ui.widget.image_tiles.TileSizeSupplier.TileSize;

/** A helper class to compute dimensions for the carousel layout. */
class TileSizeSupplier implements Supplier<TileSize> {
    /** Contains details to be used by the grid layout when placing items. */
    public static class TileSize {
        public int width;
        public int interTilePadding;
    }

    private final TileSize mComputedTileSize;
    private final Resources mResources;
    private final int mIdealTileWidth;
    private final int mInterTilePadding;
    private final int mStartMargin;

    /** Constructor. */
    public TileSizeSupplier(Context context) {
        mComputedTileSize = new TileSize();
        mResources = context.getResources();
        mIdealTileWidth = mResources.getDimensionPixelOffset(R.dimen.tile_ideal_width);
        mInterTilePadding =
                mResources.getDimensionPixelOffset(R.dimen.tile_grid_inter_tile_padding);
        mStartMargin = mResources.getDimensionPixelOffset(R.dimen.tile_grid_inter_tile_padding);
        recompute();
    }

    @Override
    public TileSize get() {
        return mComputedTileSize;
    }

    /**
     * Given a desired cell width, computes the actual item width feasible. Should be invoked after
     * a orientation change as well.
     */
    public void recompute() {
        double idealSpanCount =
                (double) (getAvailableWidth() + mInterTilePadding)
                        / (mIdealTileWidth + mInterTilePadding);
        double delta = idealSpanCount - Math.floor(idealSpanCount);

        // For carousel, we need to have the last cell peeking out of the screen. So clamp the last
        // cell between 30% and 70%.
        delta = Math.max(0.3, Math.min(0.7, delta));
        double adjustedSpanCount = Math.floor(idealSpanCount) + delta;

        double tileWidthToUse =
                (getAvailableWidth() - mInterTilePadding * Math.floor(adjustedSpanCount))
                        / adjustedSpanCount;

        mComputedTileSize.interTilePadding = mInterTilePadding;
        mComputedTileSize.width = (int) tileWidthToUse;
    }

    private int getAvailableWidth() {
        // TODO(shaktisahu): Cap this for tablet and landscape to 600dp.
        return mResources.getDisplayMetrics().widthPixels - 2 * mStartMargin;
    }
}
