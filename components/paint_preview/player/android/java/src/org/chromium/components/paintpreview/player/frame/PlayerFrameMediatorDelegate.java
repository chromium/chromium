// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

import android.graphics.Bitmap;
import android.graphics.Matrix;
import android.graphics.Rect;
import android.util.Size;

/** API of the PlayerFrameMediator to helper classes. */
public interface PlayerFrameMediatorDelegate {
    /** Gets the visual viewport of the player. */
    public PlayerFrameViewport getViewport();

    /** Gets the size of the content shown in the mediator. */
    public Size getContentSize();

    /** Gets the min scale factor at the last computed viewport width. */
    public float getMinScaleFactor();

    /**
     * Triggers an update of the visual contents of the PlayerFrameView. This fetches updates the
     * model and fetches any new bitmaps asynchronously.
     * @param scaleChanged Indicates that the scale changed so all current bitmaps need to be
     *     discarded.
     */
    void updateVisuals(boolean scaleChanged);

    /**
     * Updates the visibility and size of subframes.
     * @param viewportRect The viewport rect to use for computing visibility.
     * @param scaleFactor The scale factor at which to compute visibility.
     */
    void updateSubframes(Rect viewportRect, float scaleFactor);

    /**
     * Sets the bitmap scale matrix and recursively sets the bitmap scale matrix of children
     * ignoring the translation portion of the transform. Also updates subframe visibility of
     * nested subframes.
     * @param bitmapScaleMatrix The bitmap scale matrix to use for the currently loaded bitmaps.
     * @param scaleFactor The scale factor to use when computing nested subframe visibility.
     */
    void setBitmapScaleMatrix(Matrix bitmapScaleMatrix, float scaleFactor);

    /**
     * Updates the scale factor of subframes. This allows a correct scale factor to be used for
     * subframes when fetching bitmaps at the new scale.
     */
    void updateScaleFactorOfAllSubframes(float scaleFactor);

    /**
     * Forcibly redraws the currently visible subframes. This prevents issues where a subframe won't
     * redraw when scaling is finished if its layout size didn't change.
     */
    void forceRedrawVisibleSubframes();

    /** Updates the bitmap matrix in the model. */
    void updateBitmapMatrix(Bitmap[][] bitmapMatrix);

    /** Update the model when the bitmap state is swapped. */
    void onSwapState();

    /** To be called when scaling is started to prevent double-buffering from swapping mid-scale. */
    void onStartScaling();

    /**
     * Offsets the bitmap scale matrix when scrolling if applicable.
     * @param dx Offset on the x-axis.
     * @param dy Offset on the y-axis.
     */
    void offsetBitmapScaleMatrix(float dx, float dy);
}
