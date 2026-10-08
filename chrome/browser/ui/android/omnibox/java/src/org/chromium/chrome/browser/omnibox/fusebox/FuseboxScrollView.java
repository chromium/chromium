// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import android.annotation.SuppressLint;
import android.content.Context;
import android.util.AttributeSet;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.ViewConfiguration;
import android.widget.ScrollView;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** A ScrollView that intercepts swipe-down gestures to dismiss the Fusebox popup. */
@NullMarked
public class FuseboxScrollView extends ScrollView {
    // Same as BottomSheetSwipeDetector: a drag must be at least twice as vertical as horizontal.
    private static final float MIN_VERTICAL_SCROLL_SLOPE = 2.0f;

    /** Listener for swipe-down gestures on the scroll view. */
    public interface OnSwipeDownListener {
        /** Called when a swipe-down gesture is detected. */
        void onSwipeDown();
    }

    private @Nullable GestureDetector mGestureDetector;
    private final int mMinFlingVelocity;

    // Whether the current gesture is a downward drag while scrolled to the top.
    // Set in onScroll, reset on ACTION_DOWN.
    private boolean mIsSwipingDown;

    @VisibleForTesting
    final GestureDetector.SimpleOnGestureListener mGestureListener =
            new GestureDetector.SimpleOnGestureListener() {
                @Override
                public boolean onScroll(
                        @Nullable MotionEvent e1,
                        MotionEvent e2,
                        float distanceX,
                        float distanceY) {
                    // distanceY is (previous - current), so it's negative when dragging down.
                    boolean isDraggingDown = distanceY < 0;
                    boolean isMostlyVertical =
                            Math.abs(distanceY) >= MIN_VERTICAL_SCROLL_SLOPE * Math.abs(distanceX);
                    boolean isScrolledToTop = getScrollY() == 0;
                    if (isDraggingDown && isMostlyVertical && isScrolledToTop) {
                        mIsSwipingDown = true;
                    }
                    return false;
                }

                @Override
                public boolean onFling(
                        @Nullable MotionEvent e1,
                        @Nullable MotionEvent e2,
                        float velocityX,
                        float velocityY) {
                    if (mIsSwipingDown && velocityY > mMinFlingVelocity && getScrollY() == 0) {
                        if (mOnSwipeDownListener != null) {
                            mOnSwipeDownListener.onSwipeDown();
                            return true;
                        }
                    }
                    return false;
                }
            };

    private @Nullable OnSwipeDownListener mOnSwipeDownListener;

    /**
     * Constructor for inflating from XML.
     *
     * @param context The application context.
     * @param attrs The attribute set.
     */
    public FuseboxScrollView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        mMinFlingVelocity = ViewConfiguration.get(context).getScaledMinimumFlingVelocity();
    }

    /**
     * Sets the listener for swipe-down gestures.
     *
     * @param listener The listener to be notified of swipe-down events.
     */
    public void setOnSwipeDownListener(@Nullable OnSwipeDownListener listener) {
        mOnSwipeDownListener = listener;
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        if (mGestureDetector == null) {
            mGestureDetector = new GestureDetector(getContext(), mGestureListener);
        }
    }

    @Override
    @SuppressLint("ClickableViewAccessibility")
    public boolean onInterceptTouchEvent(MotionEvent ev) {
        if (ev.getActionMasked() == MotionEvent.ACTION_DOWN) {
            mIsSwipingDown = false;
        }
        // Claim downward swipes before a child (e.g. a horizontal carousel) does. ScrollView
        // won't claim them itself when its content doesn't scroll.
        return handleFling(ev) || mIsSwipingDown || super.onInterceptTouchEvent(ev);
    }

    @Override
    @SuppressLint("ClickableViewAccessibility")
    public boolean onTouchEvent(MotionEvent ev) {
        return handleFling(ev) || super.onTouchEvent(ev);
    }

    private boolean handleFling(MotionEvent ev) {
        return mOnSwipeDownListener != null
                && mGestureDetector != null
                && mGestureDetector.onTouchEvent(ev);
    }
}
