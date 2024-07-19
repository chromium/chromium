// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import android.content.Context;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.VelocityTracker;

import org.chromium.base.MathUtils;
import org.chromium.base.ThreadUtils;

/**
 * A class that determines whether a sequence of motion events is a valid swipe in the context of a
 * bottom sheet. The {@link SwipeableBottomSheet} that this class is built with provides information
 * useful to determining if a swipe is valid. This class does not move the sheet itself, it only
 * provides information on if/where it should move and whether it should animate. The
 * {@link SwipeableBottomSheet} is responsible for applying the changes to the relevant views. Each
 * swipe or fling is converted into a sequence of calls to
 * {@link SwipeableBottomSheet#setSheetOffset(float, boolean)}.
 */
class BottomSheetSwipeDetector extends GestureDetector.SimpleOnGestureListener {
    /** The minimum y/x ratio that a scroll must have to be considered vertical. */
    private static final float MIN_VERTICAL_SCROLL_SLOPE = 2.0f;

    /**
     * The base duration of the settling animation of the sheet. 218 ms is a spec for material
     * design (this is the minimum time a user is guaranteed to pay attention to something).
     */
    public static final long BASE_ANIMATION_DURATION_MS = 218;

    /** For detecting scroll and fling events on the bottom sheet. */
    private final GestureDetector mGestureDetector;

    /** An interface for retrieving information from a bottom sheet. */
    private final SwipeableBottomSheet mSheetDelegate;

    /** Track the velocity of the user's scrolls to determine up or down direction. */
    private VelocityTracker mVelocityTracker;

    /** Whether or not the user is scrolling the bottom sheet. */
    private boolean mIsScrolling;

    /**
     * An interface for views that are swipable from the bottom of the screen. This interface
     * assumes that any part of the bottom sheet visible at the peeking state is the toolbar.
     */
    public interface SwipeableBottomSheet {
        /** @return Whether the content being shown in the sheet is scrolled to the top. */
        boolean isContentScrolledToTop();

        /**
         * Gets the sheet's offset from the bottom of the screen.
         * @return The sheet's distance from the bottom of the screen.
         */
        float getCurrentOffsetPx();

        /**
         * Gets the minimum offset of the bottom sheet.
         * @return The min offset.
         */
        float getMinOffsetPx();

        /**
         * Gets the maximum offset of the bottom sheet.
         * @return The max offset.
         */
        float getMaxOffsetPx();

        /**
         * @param event The motion event to test.
         * @return Whether the provided motion event is inside the toolbar.
         */
        boolean isTouchEventInToolbar(MotionEvent event);

        /**
         * Check if a particular gesture or touch event should move the bottom sheet when in peeking
         * mode. If the "chrome-home-swipe-logic" flag is not set this function returns true.
         * @param initialDownEvent The event that started the scroll.
         * @param currentEvent The current motion event.
         * @return True if the bottom sheet should move.
         */
        boolean shouldGestureMoveSheet(MotionEvent initialDownEvent, MotionEvent currentEvent);

        /**
         * Set the sheet's offset.
         * @param offset The target offset.
         * @param shouldAnimate Whether the sheet should animate to that position.
         */
        void setSheetOffset(float offset, boolean shouldAnimate);
    }

    /**
     * This class is responsible for detecting swipe and scroll events on the bottom sheet or
     * ignoring them when appropriate.
     */
    private class SwipeGestureListener extends GestureDetector.SimpleOnGestureListener {
        @Override
        public boolean onDown(MotionEvent e) {
            if (e == null) return false;
            return mSheetDelegate.shouldGestureMoveSheet(e, e);
        }

        @Override
        public boolean onScroll(MotionEvent e1, MotionEvent e2, float distanceX, float distanceY) {
            if (e1 == null || !mSheetDelegate.shouldGestureMoveSheet(e1, e2)) return false;

            // Only start scrolling if the scroll is up or down. If the user is already scrolling,
            // continue moving the sheet.
            float slope =
                    Math.abs(distanceX) > 0f
                            ? Math.abs(distanceY) / Math.abs(distanceX)
                            : MIN_VERTICAL_SCROLL_SLOPE;
            if (!mIsScrolling && slope < MIN_VERTICAL_SCROLL_SLOPE) {
                mVelocityTracker.clear();
                return false;
            }

            mVelocityTracker.addMovement(e2);

            boolean isSheetInMaxPosition =
                    MathUtils.areFloatsEqual(
                            mSheetDelegate.getCurrentOffsetPx(), mSheetDelegate.getMaxOffsetPx());

            // Allow the bottom sheet's content to be scrolled up without dragging the sheet down.
            if (!mSheetDelegate.isTouchEventInToolbar(e2)
                    && isSheetInMaxPosition
                    && !mSheetDelegate.isContentScrolledToTop()) {
                return false;
            }

            // If the sheet is in the max position, don't move the sheet if the scroll is upward.
            // Instead, allow the sheet's content to handle it if it needs to.
            if (isSheetInMaxPosition && distanceY > 0) return false;

            boolean isSheetInMinPosition =
                    MathUtils.areFloatsEqual(
                            mSheetDelegate.getCurrentOffsetPx(), mSheetDelegate.getMinOffsetPx());

            // Similarly, if the sheet is in the min position, don't move if the scroll is downward.
            if (isSheetInMinPosition && distanceY < 0) return false;

            float newOffset = mSheetDelegate.getCurrentOffsetPx() + distanceY;

            mIsScrolling = true;

            mSheetDelegate.setSheetOffset(
                    MathUtils.clamp(
                            newOffset,
                            mSheetDelegate.getMinOffsetPx(),
                            mSheetDelegate.getMaxOffsetPx()),
                    false);

            return true;
        }

        @Override
        public void onLongPress(MotionEvent e) {
            mIsScrolling = false;
        }

        @Override
        public boolean onFling(MotionEvent e1, MotionEvent e2, float velocityX, float velocityY) {
            if (e1 == null || !mSheetDelegate.shouldGestureMoveSheet(e1, e2) || !mIsScrolling) {
                return false;
            }

            mIsScrolling = false;

            float newOffset = mSheetDelegate.getCurrentOffsetPx() + getFlingDistance(-velocityY);

            mSheetDelegate.setSheetOffset(
                    MathUtils.clamp(
                            newOffset,
                            mSheetDelegate.getMinOffsetPx(),
                            mSheetDelegate.getMaxOffsetPx()),
                    true);

            return true;
        }
    }

    /**
     * Default constructor.
     * @param context A context for the GestureDetector this class uses.
     * @param delegate A SwipeableBottomSheet that processes swipes.
     */
    public BottomSheetSwipeDetector(Context context, SwipeableBottomSheet delegate) {
        mGestureDetector =
                new GestureDetector(
                        context, new SwipeGestureListener(), ThreadUtils.getUiThreadHandler());
        mGestureDetector.setIsLongpressEnabled(true);

        mSheetDelegate = delegate;
        mVelocityTracker = VelocityTracker.obtain();
    }

    /**
     * Test whether or not a motion event should be intercepted by this class.
     * @param e The motion event to test.
     * @return Whether or not the event was intercepted.
     */
    public boolean onInterceptTouchEvent(MotionEvent e) {
        // The incoming motion event may have been adjusted by the view sending it down. Create a
        // motion event with the raw (x, y) coordinates of the original so the gesture detector
        // functions properly.
        mGestureDetector.onTouchEvent(createRawMotionEvent(e));

        return mIsScrolling;
    }

    /**
     * Process a motion event.
     * @param e The motion event to process.
     * @return Whether or not the motion event was used.
     */
    public boolean onTouchEvent(MotionEvent e) {
        // The down event is interpreted above in onInterceptTouchEvent, it does not need to be
        // interpreted a second time.
        if (e.getActionMasked() != MotionEvent.ACTION_DOWN) {
            mGestureDetector.onTouchEvent(createRawMotionEvent(e));
        }

        // If the user is scrolling and the event is a cancel or up action, update scroll state and
        // return. Fling should have already cleared the mIsScrolling flag, the following is for the
        // non-fling release.
        if (mIsScrolling
                && (e.getActionMasked() == MotionEvent.ACTION_UP
                        || e.getActionMasked() == MotionEvent.ACTION_CANCEL)) {
            mIsScrolling = false;

            mVelocityTracker.computeCurrentVelocity(1000);

            float newOffset =
                    mSheetDelegate.getCurrentOffsetPx()
                            + getFlingDistance(-mVelocityTracker.getYVelocity());

            mSheetDelegate.setSheetOffset(
                    MathUtils.clamp(
                            newOffset,
                            mSheetDelegate.getMinOffsetPx(),
                            mSheetDelegate.getMaxOffsetPx()),
                    true);
        }

        return true;
    }

    /** @return Whether or not a gesture is currently being detected as a scroll. */
    public boolean isScrolling() {
        return mIsScrolling;
    }

    void setShouldLongPressMoveSheet(boolean shouldMoveSheet) {
        mGestureDetector.setIsLongpressEnabled(!shouldMoveSheet);
    }

    /**
     * Creates an unadjusted version of a MotionEvent.
     *
     * @param e The original event.
     * @return The unadjusted version of the event.
     */
    private MotionEvent createRawMotionEvent(MotionEvent e) {
        MotionEvent rawEvent = MotionEvent.obtain(e);
        rawEvent.setLocation(e.getRawX(), e.getRawY());
        return rawEvent;
    }

    /**
     * Gets the distance of a fling based on the velocity and the base animation time. This formula
     * assumes the deceleration curve is quadratic (t^2), hence the displacement formula should be:
     * displacement = initialVelocity * duration / 2.
     * @param velocity The velocity of the fling.
     * @return The distance the fling would cover.
     */
    private float getFlingDistance(float velocity) {
        // This includes conversion from seconds to ms.
        return velocity * BASE_ANIMATION_DURATION_MS / 2000f;
    }
}
