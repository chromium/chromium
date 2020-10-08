// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.os.Handler;
import android.util.Size;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.util.HashSet;
import java.util.Set;

/**
 * Given a viewport {@link Rect} and a matrix of {@link Bitmap} tiles, this class draws the bitmaps
 * on a {@link Canvas}.
 */
class PlayerFrameBitmapPainter {
    private Size mTileSize;
    private CompressibleBitmap[][] mBitmapMatrix;
    private Rect mViewPort = new Rect();
    private Rect mDrawBitmapSrc = new Rect();
    private Rect mDrawBitmapDst = new Rect();
    private Runnable mInvalidateCallback;
    private Runnable mFirstPaintListener;
    private Handler mHandler = new Handler();

    // The following sets should only be modified on {@link mHandler} or UI thread.

    /**
     * Tracks which bitmaps are used in each {@link onDraw(Canvas)} call. Bitmaps in this set were
     * in the viewport for the last draw. Bitmaps that are not in this set but are in
     * {@link mInflatedBitmaps} are discarded at the end of {@link onDraw(Canvas)}.
     */
    private Set<CompressibleBitmap> mBitmapsToKeep = new HashSet<>();
    /**
     * Keeps track of which bitmaps are queued for inflation. Each bitmap in this list will be
     * inflated. Although if the bitmap leaves the viewport before being added to this set it
     * will be discarded in the next {@link onDraw(Canvas)}.
     */
    private Set<CompressibleBitmap> mInflatingBitmaps = new HashSet<>();
    /**
     * Keeps track of which bitmaps are inflated. Bitmaps in this set are cached in inflated form
     * to keep {@link onDraw(Canvas)} performant.
     */
    private Set<CompressibleBitmap> mInflatedBitmaps = new HashSet<>();

    PlayerFrameBitmapPainter(@NonNull Runnable invalidateCallback,
            @Nullable Runnable firstPaintListener) {
        mInvalidateCallback = invalidateCallback;
        mFirstPaintListener = firstPaintListener;
    }

    void updateTileDimensions(Size tileDimensions) {
        mTileSize = tileDimensions;
    }

    void updateViewPort(int left, int top, int right, int bottom) {
        mViewPort.set(left, top, right, bottom);
        mInvalidateCallback.run();
    }

    void updateBitmapMatrix(CompressibleBitmap[][] bitmapMatrix) {
        mBitmapMatrix = bitmapMatrix;
        mInvalidateCallback.run();
    }

    /**
     * Draws bitmaps on a given {@link Canvas} for the current viewport.
     */
    void onDraw(Canvas canvas) {
        if (mBitmapMatrix == null) return;

        if (mViewPort.isEmpty()) return;

        if (mTileSize.getWidth() <= 0 || mTileSize.getHeight() <= 0) return;

        final int rowStart = mViewPort.top / mTileSize.getHeight();
        int rowEnd = (int) Math.ceil((double) mViewPort.bottom / mTileSize.getHeight());
        final int colStart = mViewPort.left / mTileSize.getWidth();
        int colEnd = (int) Math.ceil((double) mViewPort.right / mTileSize.getWidth());

        rowEnd = Math.min(rowEnd, mBitmapMatrix.length);
        colEnd = Math.min(colEnd, rowEnd >= 1 ? mBitmapMatrix[rowEnd - 1].length : 0);

        mInflatingBitmaps.clear();
        mBitmapsToKeep.clear();
        for (int row = rowStart; row < rowEnd; row++) {
            for (int col = colStart; col < colEnd; col++) {
                CompressibleBitmap compressibleBitmap = mBitmapMatrix[row][col];
                if (compressibleBitmap == null) continue;
                mBitmapsToKeep.add(compressibleBitmap);

                if (!compressibleBitmap.lock()) {
                    // Re-issue an invalidation on the chance access was blocked due to being
                    // discarded.
                    mHandler.post(mInvalidateCallback);
                    continue;
                }

                Bitmap tileBitmap = compressibleBitmap.getBitmap();
                if (tileBitmap == null) {
                    compressibleBitmap.unlock();
                    mInflatingBitmaps.add(compressibleBitmap);
                    compressibleBitmap.inflateInBackground(inflatedBitmap -> {
                        final boolean inflated = inflatedBitmap.getBitmap() != null;
                        // Handler is on the UI thread so the needed bitmaps will be the last
                        // set of bitmaps requested.
                        mHandler.post(() -> {
                            if (inflated) {
                                mInflatedBitmaps.add(inflatedBitmap);
                            }
                            mInflatingBitmaps.remove(inflatedBitmap);
                            if (mInflatingBitmaps.isEmpty()) {
                                mInvalidateCallback.run();
                            }
                        });
                    });
                    continue;
                } else {
                    mInflatedBitmaps.add(compressibleBitmap);
                }

                // Calculate the portion of this tileBitmap that is visible in mViewPort.
                int bitmapLeft = Math.max(mViewPort.left - (col * mTileSize.getWidth()), 0);
                int bitmapTop = Math.max(mViewPort.top - (row * mTileSize.getHeight()), 0);
                int bitmapRight = Math.min(mTileSize.getWidth(),
                        bitmapLeft + mViewPort.right - (col * mTileSize.getWidth()));
                int bitmapBottom = Math.min(mTileSize.getHeight(),
                        bitmapTop + mViewPort.bottom - (row * mTileSize.getHeight()));
                mDrawBitmapSrc.set(bitmapLeft, bitmapTop, bitmapRight, bitmapBottom);

                // Calculate the portion of the canvas that tileBitmap is gonna be drawn on.
                int canvasLeft = Math.max((col * mTileSize.getWidth()) - mViewPort.left, 0);
                int canvasTop = Math.max((row * mTileSize.getHeight()) - mViewPort.top, 0);
                int canvasRight = canvasLeft + mDrawBitmapSrc.width();
                int canvasBottom = canvasTop + mDrawBitmapSrc.height();
                mDrawBitmapDst.set(canvasLeft, canvasTop, canvasRight, canvasBottom);

                canvas.drawBitmap(tileBitmap, mDrawBitmapSrc, mDrawBitmapDst, null);
                compressibleBitmap.unlock();
                if (mFirstPaintListener != null) {
                    mFirstPaintListener.run();
                    mFirstPaintListener = null;
                }
            }
        }
        for (CompressibleBitmap inflatedBitmap : mInflatedBitmaps) {
            if (mBitmapsToKeep.contains(inflatedBitmap)) continue;

            inflatedBitmap.discardBitmap();
        }
        mInflatedBitmaps.clear();
        mInflatedBitmaps.addAll(mBitmapsToKeep);
    }
}
