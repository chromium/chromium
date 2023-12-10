// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

import android.util.Size;

import org.chromium.base.UnguessableToken;
import org.chromium.components.paintpreview.player.PlayerCompositorDelegate;

/** Class for managing which bitmap state is shown. */
public class PlayerFrameBitmapStateController {
    private PlayerFrameBitmapState mLoadingBitmapState;
    private PlayerFrameBitmapState mVisibleBitmapState;

    private final UnguessableToken mGuid;
    private final PlayerFrameViewport mViewport;
    private final Size mContentSize;
    private final PlayerCompositorDelegate mCompositorDelegate;
    private final PlayerFrameMediatorDelegate mMediatorDelegate;

    PlayerFrameBitmapStateController(
            UnguessableToken guid,
            PlayerFrameViewport viewport,
            Size contentSize,
            PlayerCompositorDelegate compositorDelegate,
            PlayerFrameMediatorDelegate mediatorDelegate) {
        mGuid = guid;
        mViewport = viewport;
        mContentSize = contentSize;
        mCompositorDelegate = compositorDelegate;
        if (mCompositorDelegate != null) {
            mCompositorDelegate.addMemoryPressureListener(this::onMemoryPressure);
        }
        mMediatorDelegate = mediatorDelegate;
    }

    void deleteAll() {
        if (mLoadingBitmapState != null) {
            mLoadingBitmapState.destroy();
            mLoadingBitmapState = null;
        }
        if (mVisibleBitmapState != null) {
            mVisibleBitmapState.destroy();
            mVisibleBitmapState = null;
        }
    }

    void destroy() {
        deleteAll();
    }

    void swapForTest() {
        swap(mLoadingBitmapState);
    }

    void onMemoryPressure() {
        if (mVisibleBitmapState == null) return;

        mVisibleBitmapState.releaseNotVisibleTiles();
        stateUpdated(mVisibleBitmapState);
    }

    /**
     * Gets the bitmap state for loading.
     * @param scaleUpdated Whether the scale was updated.
     * @return The bitmap state to load new bitmaps to.
     */
    PlayerFrameBitmapState getBitmapState(boolean scaleUpdated) {
        // Prefer mLoadingBitmapState if one exist. Otherwise use mVisibleBitmapState.
        PlayerFrameBitmapState activeLoadingState =
                (mLoadingBitmapState == null) ? mVisibleBitmapState : mLoadingBitmapState;
        if (scaleUpdated || activeLoadingState == null) {
            invalidateLoadingBitmaps();
            Size tileSize = mViewport.getBitmapTileSize();
            mLoadingBitmapState =
                    new PlayerFrameBitmapState(
                            mGuid,
                            tileSize.getWidth(),
                            tileSize.getHeight(),
                            mViewport.getScale(),
                            mContentSize,
                            mCompositorDelegate,
                            this);
            if (mVisibleBitmapState == null) {
                mLoadingBitmapState.skipWaitingForVisibleBitmaps();
                swap(mLoadingBitmapState);
                activeLoadingState = mVisibleBitmapState;
            } else {
                activeLoadingState = mLoadingBitmapState;
            }
        }
        return activeLoadingState;
    }

    /**
     * Swaps the state to be new state.
     * @param newState The new visible bitmap state.
     */
    void swap(PlayerFrameBitmapState newState) {
        assert mLoadingBitmapState == newState;
        PlayerFrameBitmapState oldState = mVisibleBitmapState;
        mVisibleBitmapState = newState;
        mLoadingBitmapState = null;
        mMediatorDelegate.onSwapState();
        // Clear the state to stop potential stragling updates. Destroy afterwards in case drawing
        // is happening concurrently somehow.
        if (oldState != null) {
            oldState.destroy();
        }
    }

    /**
     * Signals the bitmap state was updated.
     * @param bitmapState The bitmap state that was updated.
     */
    void stateUpdated(PlayerFrameBitmapState bitmapState) {
        if (isVisible(bitmapState)) {
            mMediatorDelegate.updateBitmapMatrix(bitmapState.getMatrix());
            return;
        }

        if (!bitmapState.isReadyToShow()) return;

        swap(bitmapState);
    }

    /** Whether the bitmap state is visible. */
    boolean isVisible(PlayerFrameBitmapState state) {
        return state == mVisibleBitmapState;
    }

    void onStartScaling() {
        if (mVisibleBitmapState == null) return;
        invalidateLoadingBitmaps();

        if (mVisibleBitmapState == null) return;

        mVisibleBitmapState.lock();
    }

    /** Invalidates loading bitmaps. */
    void invalidateLoadingBitmaps() {
        if (mLoadingBitmapState == null) return;

        // Invalidate an in-progress load if there is one. We only want one new scale factor fetched
        // at a time. NOTE: we clear then null as the bitmap callbacks still hold a reference to the
        // state so it won't be GC'd right away.
        mLoadingBitmapState.destroy();
        mLoadingBitmapState = null;
    }
}
