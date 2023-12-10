// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.layouts;

import android.content.Context;
import android.view.MotionEvent;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** A class intended to process input events for non-android views. */
public abstract class EventFilter {
    /** The type of input event that will be intercepted and handled by the filter. */
    @IntDef({EventType.UNKNOWN, EventType.TOUCH, EventType.HOVER})
    @Retention(RetentionPolicy.SOURCE)
    public @interface EventType {
        int UNKNOWN = 0;
        int TOUCH = 1;
        int HOVER = 2;
    }

    protected final float mPxToDp;
    private boolean mSimulateIntercepting;

    private boolean mAutoOffset;
    protected float mCurrentMotionOffsetX;
    protected float mCurrentMotionOffsetY;

    /**
     * Creates a {@link EventFilter}.
     * @param context A {@link Context} instance.
     */
    public EventFilter(Context context) {
        this(context, true);
    }

    /**
     * Creates a {@link EventFilter}.
     * @param autoOffset Whether or not to automatically offset touch events.
     */
    public EventFilter(Context context, boolean autoOffset) {
        mPxToDp = 1.0f / context.getResources().getDisplayMetrics().density;
        mAutoOffset = autoOffset;
    }

    /**
     * @see android.view.ViewGroup#onInterceptTouchEvent(android.view.MotionEvent)
     * @param event             The {@link MotionEvent} that started the gesture to be evaluated.
     * @param isKeyboardShowing Whether the keyboard is currently showing.
     * @return                  Whether the filter is going to intercept events.
     */
    public final boolean onInterceptTouchEvent(MotionEvent event, boolean isKeyboardShowing) {
        MotionEvent sentEvent = event;
        if (mAutoOffset && (mCurrentMotionOffsetX != 0 || mCurrentMotionOffsetY != 0)) {
            sentEvent = MotionEvent.obtain(event);
            sentEvent.offsetLocation(mCurrentMotionOffsetX, mCurrentMotionOffsetY);
        }
        boolean consumed = onInterceptTouchEventInternal(sentEvent, isKeyboardShowing);
        if (sentEvent != event) sentEvent.recycle();
        return consumed;
    }

    /**
     * @see android.view.ViewGroup#onInterceptHoverEvent(android.view.MotionEvent)
     * @param event The {@link MotionEvent} that started the gesture to be evaluated.
     * @return      Whether the filter is going to intercept events.
     */
    public final boolean onInterceptHoverEvent(MotionEvent event) {
        MotionEvent sentEvent = event;
        if (mAutoOffset && (mCurrentMotionOffsetX != 0 || mCurrentMotionOffsetY != 0)) {
            sentEvent = MotionEvent.obtain(event);
            sentEvent.offsetLocation(mCurrentMotionOffsetX, mCurrentMotionOffsetY);
        }
        boolean consumed = onInterceptHoverEventInternal(sentEvent);
        if (sentEvent != event) sentEvent.recycle();
        return consumed;
    }

    /**
     * Sets the offset to apply to MotionEvents.
     *
     * @param offsetX How much to offset the X motion events in pixels.
     * @param offsetY How much to offset the Y motion events in pixels.
     */
    public void setCurrentMotionEventOffsets(float offsetX, float offsetY) {
        mCurrentMotionOffsetX = offsetX;
        mCurrentMotionOffsetY = offsetY;
    }

    /**
     * This function has a fairly uncommon behavior, if you change anything please checkout the
     * java-doc here:
     * @see android.view.ViewGroup#onInterceptTouchEvent(android.view.MotionEvent)
     * @param event             The {@link MotionEvent} that started the gesture to be evaluated.
     * @param isKeyboardShowing Whether the keyboard is currently showing.
     * @return                  Whether the filter is going to intercept events.
     */
    protected abstract boolean onInterceptTouchEventInternal(
            MotionEvent event, boolean isKeyboardShowing);

    /**
     * This function has a fairly uncommon behavior, if you change anything please checkout the
     * java-doc here:
     * @see android.view.ViewGroup#onInterceptHoverEvent(android.view.MotionEvent)
     * @param event The {@link MotionEvent} that started the hover action.
     * @return      Whether the filter is going to intercept events.
     */
    protected abstract boolean onInterceptHoverEventInternal(MotionEvent event);

    /**
     * @see android.view.ViewGroup#onTouchEvent(android.view.MotionEvent)
     * @param event The {@link MotionEvent} that started the gesture to be evaluated.
     * @return      Whether the filter handled the event.
     */
    public final boolean onTouchEvent(MotionEvent event) {
        if (mAutoOffset) event.offsetLocation(mCurrentMotionOffsetX, mCurrentMotionOffsetY);
        return onTouchEventInternal(event);
    }

    /**
     * @see android.view.ViewGroup#onHoverEvent(android.view.MotionEvent)
     * @param event The {@link MotionEvent} that started the hover action.
     * @return      Whether the filter handled the event.
     */
    public final boolean onHoverEvent(MotionEvent event) {
        if (mAutoOffset) event.offsetLocation(mCurrentMotionOffsetX, mCurrentMotionOffsetY);
        return onHoverEventInternal(event);
    }

    /**
     * @see android.view.ViewGroup#onTouchEvent(android.view.MotionEvent)
     * @param event The {@link MotionEvent} that started the gesture to be evaluated.
     * @return      Whether the filter handled the event.
     */
    protected abstract boolean onTouchEventInternal(MotionEvent event);

    /**
     * @see android.view.ViewGroup#onHoverEvent(android.view.MotionEvent)
     * @param event The {@link MotionEvent} that started the hover action.
     * @return      Whether the filter handled the event.
     */
    protected abstract boolean onHoverEventInternal(MotionEvent event);

    /**
     * Simulates an event for testing purpose. This will call onInterceptTouchEvent and
     * onTouchEvent appropriately.
     * @param event             The {@link MotionEvent} that started the hover action.
     * @param isKeyboardShowing Whether the keyboard is currently showing.
     * @return                  Whether the filter handled the event.
     */
    @VisibleForTesting
    public boolean simulateTouchEvent(MotionEvent event, boolean isKeyboardShowing) {
        if (event.getActionMasked() == MotionEvent.ACTION_DOWN || !mSimulateIntercepting) {
            mSimulateIntercepting = onInterceptTouchEvent(event, isKeyboardShowing);
        }
        return onTouchEvent(event);
    }
}
