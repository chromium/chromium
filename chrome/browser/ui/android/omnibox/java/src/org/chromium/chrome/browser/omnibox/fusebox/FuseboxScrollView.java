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
    /** Listener for swipe-down gestures on the scroll view. */
    public interface OnSwipeDownListener {
        /** Called when a swipe-down gesture is detected. */
        void onSwipeDown();
    }

    private final GestureDetector mGestureDetector;
    private final int mMinFlingVelocity;

    @VisibleForTesting
    final GestureDetector.SimpleOnGestureListener mGestureListener =
            new GestureDetector.SimpleOnGestureListener() {
                @Override
                public boolean onFling(
                        @Nullable MotionEvent e1,
                        @Nullable MotionEvent e2,
                        float velocityX,
                        float velocityY) {
                    if (velocityY > mMinFlingVelocity && getScrollY() == 0) {
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
        mGestureDetector = new GestureDetector(context, mGestureListener);
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
    @SuppressLint("ClickableViewAccessibility")
    public boolean onInterceptTouchEvent(MotionEvent ev) {
        return handleFling(ev) || super.onInterceptTouchEvent(ev);
    }

    @Override
    @SuppressLint("ClickableViewAccessibility")
    public boolean onTouchEvent(MotionEvent ev) {
        return handleFling(ev) || super.onTouchEvent(ev);
    }

    private boolean handleFling(MotionEvent ev) {
        return mOnSwipeDownListener != null && mGestureDetector.onTouchEvent(ev);
    }
}
