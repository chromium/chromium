// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

/** Dispatches gesture events to the correct controllers. */
public class PlayerFrameGestureDetectorDelegate {
    private final PlayerFrameScaleController mScaleController;
    private final PlayerFrameScrollController mScrollController;
    private final PlayerFrameViewDelegate mViewDelegate;

    PlayerFrameGestureDetectorDelegate(
            PlayerFrameScaleController scaleController,
            PlayerFrameScrollController scrollController,
            PlayerFrameViewDelegate viewDelegate) {
        mScaleController = scaleController;
        mScrollController = scrollController;
        mViewDelegate = viewDelegate;
    }

    /**
     * Called when a scroll gesture is performed.
     * @param distanceX Horizontal scroll values in pixels.
     * @param distanceY Vertical scroll values in pixels.
     * @return Whether this scroll event was consumed.
     */
    boolean scrollBy(float distanceX, float distanceY) {
        return mScrollController.scrollBy(distanceX, distanceY);
    }

    /**
     * Called when a fling gesture is performed.
     * @param velocityX Horizontal velocity value in pixels.
     * @param velocityY Vertical velocity value in pixels.
     * @return Whether this fling was consumed.
     */
    boolean onFling(float velocityX, float velocityY) {
        return mScrollController.onFling(velocityX, velocityY);
    }

    /** Called when a gesture is released. */
    void onRelease() {
        mScrollController.onRelease();
    }

    /**
     * Called when a scale gesture is performed.
     * @return Whether this scale event was consumed.
     */
    boolean scaleBy(float scaleFactor, float focalPointX, float focalPointY) {
        return mScaleController.scaleBy(scaleFactor, focalPointX, focalPointY);
    }

    /**
     * Called when a scale gesture is finished.
     * @return Whether this scale event was consumed.
     */
    boolean scaleFinished(float scaleFactor, float focalPointX, float focalPointY) {
        return mScaleController.scaleFinished(scaleFactor, focalPointX, focalPointY);
    }

    /**
     * Called when a single tap gesture is performed.
     * @param x X coordinate of the point clicked.
     * @param y Y coordinate of the point clicked.
     */
    void onTap(int x, int y) {
        mViewDelegate.onTap(x, y, false);
    }

    /**
     * Called when a long press gesture is performed.
     * @param x X coordinate of the point clicked.
     * @param y Y coordinate of the point clicked.
     */
    void onLongPress(int x, int y) {
        mViewDelegate.onLongPress(x, y);
    }
}
