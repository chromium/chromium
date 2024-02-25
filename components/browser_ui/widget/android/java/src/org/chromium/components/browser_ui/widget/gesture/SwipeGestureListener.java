// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.gesture;

import android.content.Context;
import android.graphics.PointF;
import android.view.GestureDetector;
import android.view.GestureDetector.SimpleOnGestureListener;
import android.view.MotionEvent;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ThreadUtils;
import org.chromium.components.browser_ui.widget.R;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Recognizes directional swipe gestures using supplied {@link MotionEvent}s.
 * The {@link SwipeHandler} callbacks will notify users when a particular gesture
 * has occurred, if the handler supports the particular direction of the swipe.
 *
 * To use this class:
 * <ul>
 *  <li>Create an instance of the {@link SwipeGestureListener} for your View.
 *  <li>In the View#onTouchEvent(MotionEvent) method ensure you call
 *          {@link #onTouchEvent(MotionEvent)}. The methods defined in your callback
 *          will be executed when the gestures occur.
 *  <li>Before trying to recognize the gesture, the class will call
 *          {@link #shouldRecognizeSwipe(MotionEvent, MotionEvent)}, which allows
 *          ignoring swipe recognition based on the MotionEvents.
 *  <li>Once a swipe gesture is detected, the class will check if the the direction
 *          is supported by calling {@link SwipeHandler#isSwipeEnabled}.
 *  <li>Override {@link #onDown(MotionEvent)} to always return true if you want to intercept the
 *          event stream from the initial #onDown event. This always returns false by default, as
 *          {@link SimpleOnGestureListener} does by default.
 * </ul>
 *
 * Internally, this class uses a {@link GestureDetector} to recognize swipe gestures.
 * For convenience, this class also extends {@link SimpleOnGestureListener} which
 * is passed to the {@link GestureDetector}. This means that this class can also be
 * used to detect simple gestures defined in {@link GestureDetector}.
 */
public class SwipeGestureListener extends SimpleOnGestureListener {
    @IntDef({
        ScrollDirection.UNKNOWN,
        ScrollDirection.LEFT,
        ScrollDirection.RIGHT,
        ScrollDirection.UP,
        ScrollDirection.DOWN
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ScrollDirection {
        int UNKNOWN = 0;
        int LEFT = 1;
        int RIGHT = 2;
        int UP = 3;
        int DOWN = 4;
    }

    public interface SwipeHandler {
        /**
         * @param direction The {@link ScrollDirection} representing the swipe direction.
         * @param ev The first down motion event triggering the swipe.
         */
        default void onSwipeStarted(@ScrollDirection int direction, MotionEvent ev) {}

        /**
         * @param current The move motion event triggering the current swipe.
         * @param tx The horizontal difference between the start and the current position in px.
         * @param ty The vertical difference between the start and the current position in px.
         * @param distanceX The distance along the X axis that has been scrolled since the last call
         *         to onScroll.
         * @param distanceY The distance along the Y axis that has been scrolled since the last call
         *         to onScroll.
         */
        default void onSwipeUpdated(
                MotionEvent current, float tx, float ty, float distanceX, float distanceY) {}

        /** @param end The last motion event canceling the swipe. */
        default void onSwipeFinished() {}

        /**
         * @param direction The {@link ScrollDirection} representing the swipe direction.
         * @param current The first down motion event triggering the swipe.
         * @param tx The horizontal difference between the start and the current position in px.
         * @param ty The vertical difference between the start and the current position in px.
         * @param velocityX The velocity of this fling measured in pixels per second along the x
         *         axis.
         * @param velocityY The velocity of this fling measured in pixels per second along the y
         *         axis.
         */
        default void onFling(
                @ScrollDirection int direction,
                MotionEvent current,
                float tx,
                float ty,
                float velocityX,
                float velocityY) {}

        /**
         * @param direction The direction of the on-going swipe.
         * @return False if this direction should be ignored.
         */
        default boolean isSwipeEnabled(@ScrollDirection int direction) {
            return true;
        }
    }

    /** The internal {@link GestureDetector} used to recognize swipe gestures. */
    private final GestureDetector mGestureDetector;

    private final PointF mMotionStartPoint = new PointF();
    @ScrollDirection private int mDirection = ScrollDirection.UNKNOWN;
    private final SwipeHandler mHandler;

    /** The threshold for a vertical swipe gesture, in px. */
    private final int mSwipeVerticalDragThreshold;

    /** The threshold for a horizontal swipe gesture, in px. */
    private final int mSwipeHorizontalDragThreshold;

    /**
     * @param context The {@link Context}.
     * @param handler The {@link SwipeHandler} to handle the swipe events.
     */
    public SwipeGestureListener(Context context, SwipeHandler handler) {
        mGestureDetector = new GestureDetector(context, this, ThreadUtils.getUiThreadHandler());
        mSwipeVerticalDragThreshold =
                context.getResources()
                        .getDimensionPixelOffset(R.dimen.swipe_vertical_drag_threshold);
        mSwipeHorizontalDragThreshold =
                context.getResources()
                        .getDimensionPixelOffset(R.dimen.swipe_horizontal_drag_threshold);
        mHandler = handler;
    }

    @VisibleForTesting
    SwipeGestureListener(
            Context context, SwipeHandler handler, int verticalThreshold, int horizontalThreshold) {
        mGestureDetector = new GestureDetector(context, this, ThreadUtils.getUiThreadHandler());
        mSwipeVerticalDragThreshold = verticalThreshold;
        mSwipeHorizontalDragThreshold = horizontalThreshold;
        mHandler = handler;
    }

    /**
     * Analyzes the given motion event by feeding it to a {@link GestureDetector}. Depending on the
     * results from the onScroll() and onFling() methods, it triggers the appropriate callbacks
     * on the {@link SwipeHandler} supplied.
     *
     * @param event The {@link MotionEvent}.
     * @return Whether the event has been consumed.
     */
    public boolean onTouchEvent(MotionEvent event) {
        boolean consumed = mGestureDetector.onTouchEvent(event);

        if (mHandler != null) {
            final int action = event.getAction();
            if ((action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL)
                    && mDirection != ScrollDirection.UNKNOWN) {
                mHandler.onSwipeFinished();
                mDirection = ScrollDirection.UNKNOWN;
                consumed = true;
            }
        }

        return consumed;
    }

    /**
     * Checks whether the swipe gestures should be recognized. If this method returns false,
     * then the whole swipe recognition process will be ignored. By default this method returns
     * true. If a more complex logic is needed, this method should be overridden.
     *
     * @param e1 The first {@link MotionEvent}.
     * @param e2 The second {@link MotionEvent}.
     * @return Whether the swipe gestures should be recognized
     */
    public boolean shouldRecognizeSwipe(MotionEvent e1, MotionEvent e2) {
        return true;
    }

    // ============================================================================================
    // Swipe Recognition Helpers
    // ============================================================================================

    // Override #onDown if necessary. See JavaDoc of this class for more details.

    @Override
    public boolean onScroll(MotionEvent e1, MotionEvent e2, float distanceX, float distanceY) {
        if (mHandler == null || e1 == null || e2 == null) return false;

        if (mDirection == ScrollDirection.UNKNOWN && shouldRecognizeSwipe(e1, e2)) {
            float tx = e2.getRawX() - e1.getRawX();
            float ty = e2.getRawY() - e1.getRawY();

            @ScrollDirection int direction = ScrollDirection.UNKNOWN;

            if (Math.abs(tx) < mSwipeHorizontalDragThreshold
                    && Math.abs(ty) < mSwipeVerticalDragThreshold) {
                return false;
            }
            if (Math.abs(tx) > Math.abs(ty)) {
                direction = tx > 0.f ? ScrollDirection.RIGHT : ScrollDirection.LEFT;
            } else {
                direction = ty > 0.f ? ScrollDirection.DOWN : ScrollDirection.UP;
            }

            if (direction != ScrollDirection.UNKNOWN && mHandler.isSwipeEnabled(direction)) {
                mDirection = direction;
                mHandler.onSwipeStarted(direction, e2);
                mMotionStartPoint.set(e2.getRawX(), e2.getRawY());
            }
        }

        if (mDirection != ScrollDirection.UNKNOWN) {
            mHandler.onSwipeUpdated(
                    e2,
                    e2.getRawX() - mMotionStartPoint.x,
                    e2.getRawY() - mMotionStartPoint.y,
                    -distanceX,
                    -distanceY);
            return true;
        }

        return false;
    }

    @Override
    public boolean onFling(MotionEvent e1, MotionEvent e2, float velocityX, float velocityY) {
        if (mHandler == null) return false;

        if (mDirection != ScrollDirection.UNKNOWN) {
            mHandler.onFling(
                    mDirection,
                    e2,
                    e2.getRawX() - mMotionStartPoint.x,
                    e2.getRawY() - mMotionStartPoint.y,
                    velocityX,
                    velocityY);
            return true;
        }

        return false;
    }
}
