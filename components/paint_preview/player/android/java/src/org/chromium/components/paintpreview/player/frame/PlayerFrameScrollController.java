// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

import android.graphics.Rect;
import android.os.Handler;
import android.util.Size;
import android.widget.OverScroller;

import androidx.annotation.Nullable;

import org.chromium.components.paintpreview.player.OverscrollHandler;

/** Handles scrolling of a frame for the paint preview player. */
public class PlayerFrameScrollController {
    /** For swipe-to-refresh logic */
    private OverscrollHandler mOverscrollHandler;

    private boolean mIsOverscrolling;
    private float mOverscrollAmount;

    /** For computing flinging. */
    private final OverScroller mScroller;

    private final Handler mScrollerHandler = new Handler();

    /** References to shared state. */
    private final PlayerFrameViewport mViewport;

    private final Size mContentSize;

    /** Interface for calling shared methods on the mediator. */
    private final PlayerFrameMediatorDelegate mMediatorDelegate;

    private final Runnable mOnScrollListener;
    private final Runnable mOnFlingListener;
    private boolean mAcceptUserInput;
    private Runnable mOnScrollCallbackForAccessibility;

    PlayerFrameScrollController(
            OverScroller scroller,
            PlayerFrameMediatorDelegate mediatorDelegate,
            @Nullable Runnable onScrollListener,
            @Nullable Runnable onFlingListener) {
        mScroller = scroller;
        mViewport = mediatorDelegate.getViewport();
        mContentSize = mediatorDelegate.getContentSize();
        mMediatorDelegate = mediatorDelegate;
        mOnScrollListener = onScrollListener;
        mOnFlingListener = onFlingListener;
        mAcceptUserInput = true;
    }

    /** Sets the overscroll-to-refresh handler that handles pull-to-refresh behavior. */
    public void setOverscrollHandler(OverscrollHandler overscrollHandler) {
        mOverscrollHandler = overscrollHandler;
    }

    /**
     * Scrolls the viewport by a delta, but stays within {@link mContentSize}.
     * @param distanceX The delta on the x-axis.
     * @param distanceY The delta on the y-axis.
     * @return Whether the scrolling was possible and viewport was updated.
     */
    public boolean scrollBy(float distanceX, float distanceY) {
        mScroller.forceFinished(true);
        boolean result = scrollByInternal(distanceX, distanceY);
        if (result && mOnScrollListener != null) mOnScrollListener.run();
        return result;
    }

    /**
     * Handles flinging of the viewport.
     *
     * @param velocityX The velocity in the x-direction.
     * @param velocityY The velocity in the y-direction.
     * @return Whether the fling was consumed.
     */
    public boolean onFling(float velocityX, float velocityY) {
        if (!mAcceptUserInput) return false;

        final float scaleFactor = mViewport.getScale();
        int scaledContentWidth = (int) (mContentSize.getWidth() * scaleFactor);
        int scaledContentHeight = (int) (mContentSize.getHeight() * scaleFactor);
        mScroller.forceFinished(true);
        Rect viewportRect = mViewport.asRect();
        mScroller.fling(
                viewportRect.left,
                viewportRect.top,
                (int) -velocityX,
                (int) -velocityY,
                0,
                scaledContentWidth - viewportRect.width(),
                0,
                scaledContentHeight - viewportRect.height());

        if (!mScroller.isFinished() && mOnFlingListener != null) mOnFlingListener.run();
        mScrollerHandler.post(this::handleFling);
        return true;
    }

    /** Called when a touch event is released to possibly trigger overscroll-to-refresh. */
    public void onRelease() {
        if (mOverscrollHandler == null || !mIsOverscrolling) return;

        mOverscrollHandler.release();
        mIsOverscrolling = false;
        mOverscrollAmount = 0.0f;
    }

    /** Enables/disables processing input events for scrolling. */
    public void setAcceptUserInput(boolean acceptUserInput) {
        mAcceptUserInput = acceptUserInput;
    }

    /** Ensures that the given {@link Rect} is visible by scrolling the viewport to include it. */
    void scrollToMakeRectVisibleForAccessibility(Rect rect) {
        if (rect == null) return;

        float scaleFactor = mViewport.getScale();
        Rect targetRect =
                new Rect(
                        (int) (rect.left * scaleFactor),
                        (int) (rect.top * scaleFactor),
                        (int) (rect.right * scaleFactor),
                        (int) (rect.bottom * scaleFactor));
        Rect viewportRect = mViewport.asRect();

        if (viewportRect.contains(targetRect)) return;

        float scrollX;
        float scrollY;

        if (targetRect.top < viewportRect.top) {
            scrollY = targetRect.top - viewportRect.top;
        } else {
            scrollY = targetRect.top + targetRect.height() - viewportRect.bottom;
        }

        if (targetRect.left < viewportRect.left) {
            scrollX = targetRect.left - viewportRect.left;
        } else {
            scrollX = targetRect.left + targetRect.width() - viewportRect.right;
        }

        scrollBy(scrollX, scrollY);
    }

    void setOnScrollCallbackForAccessibility(Runnable onScrollCallback) {
        mOnScrollCallbackForAccessibility = onScrollCallback;
    }

    private boolean maybeHandleOverscroll(float distanceY) {
        if (mOverscrollHandler == null || mViewport.getTransY() >= 1f) return false;

        // Ignore if there is no active overscroll and the direction is down.
        if (!mIsOverscrolling && distanceY <= 0) return false;

        // TODO(crbug.com/40137904): Propagate this state to child mediators to
        // support easing.
        mOverscrollAmount += distanceY;

        // If the overscroll is completely eased off the cancel the event.
        if (mOverscrollAmount <= 0) {
            mIsOverscrolling = false;
            mOverscrollHandler.reset();
            return false;
        }

        // Start the overscroll event if the scroll direction is correct and one isn't active.
        if (!mIsOverscrolling && distanceY > 0) {
            mOverscrollAmount = distanceY;
            mIsOverscrolling = mOverscrollHandler.start();
        }
        mOverscrollHandler.pull(distanceY);
        return mIsOverscrolling;
    }

    private boolean scrollByInternal(float distanceX, float distanceY) {
        if (!mAcceptUserInput) return false;

        if (maybeHandleOverscroll(-distanceY)) return true;

        int validDistanceX = 0;
        int validDistanceY = 0;
        final float scaleFactor = mViewport.getScale();
        float scaledContentWidth = mContentSize.getWidth() * scaleFactor;
        float scaledContentHeight = mContentSize.getHeight() * scaleFactor;

        Rect viewportRect = mViewport.asRect();
        if (viewportRect.left > 0 && distanceX < 0) {
            validDistanceX = (int) Math.max(distanceX, -1f * viewportRect.left);
        } else if (viewportRect.right < scaledContentWidth && distanceX > 0) {
            validDistanceX = (int) Math.min(distanceX, scaledContentWidth - viewportRect.right);
        }
        if (viewportRect.top > 0 && distanceY < 0) {
            validDistanceY = (int) Math.max(distanceY, -1f * viewportRect.top);
        } else if (viewportRect.bottom < scaledContentHeight && distanceY > 0) {
            validDistanceY = (int) Math.min(distanceY, scaledContentHeight - viewportRect.bottom);
        }

        if (validDistanceX == 0 && validDistanceY == 0) {
            return false;
        }

        mMediatorDelegate.offsetBitmapScaleMatrix(validDistanceX, validDistanceY);
        mViewport.offset(validDistanceX, validDistanceY);
        mMediatorDelegate.updateVisuals(false);
        if (mOnScrollCallbackForAccessibility != null) mOnScrollCallbackForAccessibility.run();
        return true;
    }

    /** Handles a fling update by computing the next scroll offset and programmatically scrolling. */
    private void handleFling() {
        if (mScroller.isFinished()) return;

        boolean shouldContinue = mScroller.computeScrollOffset();
        int deltaX = mScroller.getCurrX() - Math.round(mViewport.getTransX());
        int deltaY = mScroller.getCurrY() - Math.round(mViewport.getTransY());
        scrollByInternal(deltaX, deltaY);

        if (shouldContinue) {
            mScrollerHandler.post(this::handleFling);
        }
    }
}
