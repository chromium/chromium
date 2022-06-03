// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

import android.graphics.Matrix;
import android.graphics.Rect;
import android.util.Size;

/**
 * Used to represent the viewport for a frame in the paint preview player. There should be one of
 * these objects per player frame and it should be shared between various classes that manipulated
 * the location. Should only be accessed on the UI thread to avoid the need for locks.
 */
public class PlayerFrameViewport {
    /** The size of the viewport. */
    private Size mViewportSize = new Size(0, 0);
    /** A 3x3 affine transformation matrix to track scale and translation. */
    private final Matrix mViewportTransform = new Matrix();
    /** Transient storage objects to avoid allocations. */
    private final Rect mViewportRect = new Rect();
    private final float[] mMatrixValues = new float[9];

    /**
     * @return the width of the viewport.
     */
    public int getWidth() {
        return mViewportSize.getWidth();
    }

    /**
     * @return the height of the viewport.
     */
    public int getHeight() {
        return mViewportSize.getHeight();
    }

    /**
     * Returns the translation of the viewport in the X direction (AKA left).
     * @return the x translation of the viewport.
     */
    float getTransX() {
        mViewportTransform.getValues(mMatrixValues);
        return mMatrixValues[Matrix.MTRANS_X];
    }

    /**
     * Returns the translation of the viewport in the Y direction (AKA top).
     * @return the y translation of the viewport.
     */
    float getTransY() {
        mViewportTransform.getValues(mMatrixValues);
        return mMatrixValues[Matrix.MTRANS_Y];
    }

    /**
     * Returns the scale at which to show contents.
     * @return a scale factor for the viewport.
     */
    public float getScale() {
        mViewportTransform.getValues(mMatrixValues);
        return mMatrixValues[Matrix.MSCALE_X]; // x and y should be identical here.
    }

    /**
     * Returns the current viewport position as a rect. Use cautiously as this is an instantaneous
     * snapshot and is not continually updated.
     * @return a rect of the current viewport.
     * */
    public Rect asRect() {
        mViewportTransform.getValues(mMatrixValues);
        final int left = Math.round(mMatrixValues[Matrix.MTRANS_X]);
        final int top = Math.round(mMatrixValues[Matrix.MTRANS_Y]);
        mViewportRect.set(
                left, top, left + mViewportSize.getWidth(), top + mViewportSize.getHeight());
        return mViewportRect;
    }

    /**
     * Sets the size of the viewport.
     * @param width The width of the viewport.
     * @param height The height of the viewport.
     */
    void setSize(int width, int height) {
        mViewportSize = new Size(width, height);
    }

    /**
     * Sets the position x, y (left, top) of the viewport.
     * @param x The left side of the viewport.
     * @param y The top of the viewport.
     */
    void setTrans(float x, float y) {
        mViewportTransform.getValues(mMatrixValues);
        mMatrixValues[Matrix.MTRANS_X] = x;
        mMatrixValues[Matrix.MTRANS_Y] = y;
        mViewportTransform.setValues(mMatrixValues);
    }

    /**
     * Sets the scale of the viewport.
     * @param scaleFactor The scale of the viewport.
     */
    void setScale(float scaleFactor) {
        mViewportTransform.getValues(mMatrixValues);
        mMatrixValues[Matrix.MSCALE_X] = scaleFactor;
        mMatrixValues[Matrix.MSCALE_Y] = scaleFactor;
        mViewportTransform.setValues(mMatrixValues);
    }

    /**
     * Offsets/shifts the viewport by a set amount.
     * @param dx The distance to offset on the x-axis.
     * @param dy The distance to offset on the y-axis.
     */
    void offset(float dx, float dy) {
        mViewportTransform.postTranslate(dx, dy);
    }

    /**
     * Affine scaling of the viewport about a focal point/pivot.
     * @param scaleFactor The amount to scale by (relative to the current scale).
     * @param focalX The x-coordinate of the focal point.
     * @param focalY The y-coordinate of the focal point.
     */
    void scale(float scaleFactor, float focalX, float focalY) {
        mViewportTransform.postScale(scaleFactor, scaleFactor, -focalX, -focalY);
    }
}
