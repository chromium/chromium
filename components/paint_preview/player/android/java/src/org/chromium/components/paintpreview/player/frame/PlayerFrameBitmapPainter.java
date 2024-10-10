// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.util.Size;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.TraceEvent;

/**
 * Given a viewport {@link Rect} and a matrix of {@link Bitmap} tiles, this class draws the bitmaps
 * on a {@link Canvas}.
 */
class PlayerFrameBitmapPainter {
    private Size mTileSize;
    private Bitmap[][] mBitmapMatrix;
    private Rect mViewPort = new Rect();
    private Rect mDrawBitmapSrc = new Rect();
    private Rect mDrawBitmapDst = new Rect();
    private Runnable mInvalidateCallback;
    private Runnable mFirstPaintListener;
    private boolean mDestroyed;

    PlayerFrameBitmapPainter(
            @NonNull Runnable invalidateCallback, @Nullable Runnable firstPaintListener) {
        mInvalidateCallback = invalidateCallback;
        mFirstPaintListener = firstPaintListener;
    }

    void updateTileDimensions(Size tileDimensions) {
        mTileSize = tileDimensions;
    }

    void updateViewPort(int left, int top, int right, int bottom) {
        if (mDestroyed) return;

        mViewPort.set(left, top, right, bottom);
        mInvalidateCallback.run();
    }

    void updateBitmapMatrix(Bitmap[][] bitmapMatrix) {
        if (mDestroyed) return;

        mBitmapMatrix = bitmapMatrix;
        mInvalidateCallback.run();
    }

    /** Draws bitmaps on a given {@link Canvas} for the current viewport. */
    void onDraw(Canvas canvas) {
        if (mDestroyed) return;

        if (mBitmapMatrix == null) return;

        if (mViewPort.isEmpty()) return;

        if (mTileSize.getWidth() <= 0 || mTileSize.getHeight() <= 0) return;
        TraceEvent.begin("PlayerFrameBitmapPainter.onDraw");

        final int rowStart = mViewPort.top / mTileSize.getHeight();
        int rowEnd = (int) Math.ceil((double) mViewPort.bottom / mTileSize.getHeight());
        final int colStart = mViewPort.left / mTileSize.getWidth();
        int colEnd = (int) Math.ceil((double) mViewPort.right / mTileSize.getWidth());

        rowEnd = Math.min(rowEnd, mBitmapMatrix.length);
        colEnd = Math.min(colEnd, rowEnd >= 1 ? mBitmapMatrix[rowEnd - 1].length : 0);

        for (int row = rowStart; row < rowEnd; row++) {
            for (int col = colStart; col < colEnd; col++) {
                Bitmap tileBitmap = mBitmapMatrix[row][col];

                // Request is ongoing or mid-replacement.
                if (tileBitmap == null || tileBitmap.isRecycled()) continue;

                // Calculate the portion of this tileBitmap that is visible in mViewPort.
                int bitmapLeft = Math.max(mViewPort.left - (col * mTileSize.getWidth()), 0);
                int bitmapTop = Math.max(mViewPort.top - (row * mTileSize.getHeight()), 0);
                int bitmapRight =
                        Math.min(
                                mTileSize.getWidth(),
                                bitmapLeft + mViewPort.right - (col * mTileSize.getWidth()));
                int bitmapBottom =
                        Math.min(
                                mTileSize.getHeight(),
                                bitmapTop + mViewPort.bottom - (row * mTileSize.getHeight()));
                mDrawBitmapSrc.set(bitmapLeft, bitmapTop, bitmapRight, bitmapBottom);

                // Calculate the portion of the canvas that tileBitmap is gonna be drawn on.
                int canvasLeft = Math.max((col * mTileSize.getWidth()) - mViewPort.left, 0);
                int canvasTop = Math.max((row * mTileSize.getHeight()) - mViewPort.top, 0);
                int canvasRight = canvasLeft + mDrawBitmapSrc.width();
                int canvasBottom = canvasTop + mDrawBitmapSrc.height();
                mDrawBitmapDst.set(canvasLeft, canvasTop, canvasRight, canvasBottom);

                canvas.drawBitmap(tileBitmap, mDrawBitmapSrc, mDrawBitmapDst, null);
                if (mFirstPaintListener != null) {
                    mFirstPaintListener.run();
                    mFirstPaintListener = null;
                }
            }
        }
        TraceEvent.end("PlayerFrameBitmapPainter.onDraw");
    }

    void destroy() {
        // Prevent future invalidation.
        mDestroyed = true;
        mBitmapMatrix = null;
    }
}
