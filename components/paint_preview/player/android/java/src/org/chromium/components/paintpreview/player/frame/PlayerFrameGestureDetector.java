// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

import android.content.Context;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;

import org.chromium.base.TraceEvent;

/**
 * Detects scroll, fling, and scale gestures on calls to {@link #onTouchEvent} and reports back to
 * the provided {@link PlayerFrameGestureDetectorDelegate}.
 */
class PlayerFrameGestureDetector
        implements GestureDetector.OnGestureListener, ScaleGestureDetector.OnScaleGestureListener {
    private GestureDetector mGestureDetector;
    private ScaleGestureDetector mScaleGestureDetector;
    private boolean mCanDetectZoom;
    private PlayerFrameGestureDetectorDelegate mDelegate;
    private PlayerFrameGestureDetector mParentGestureDetector;

    /**
     * Last horizontal scroll distance that was detected by this {@link PlayerFrameGestureDetector}
     * and consumed by {@link #mParentGestureDetector}.
     */
    private float mLastParentScrollX;

    /**
     * Last vertical scroll distance that was detected by this {@link PlayerFrameGestureDetector}
     * and consumed by {@link #mParentGestureDetector}.
     */
    private float mLastParentScrollY;

    /**
     * @param context Used for initializing {@link GestureDetector} and
     * {@link ScaleGestureDetector}.
     * @param canDetectZoom Whether this {@link PlayerFrameGestureDetector} should detect scale
     * gestures.
     * @param delegate The delegate used when desired gestured are detected.
     */
    PlayerFrameGestureDetector(
            Context context, boolean canDetectZoom, PlayerFrameGestureDetectorDelegate delegate) {
        mGestureDetector = new GestureDetector(context, this);
        mScaleGestureDetector = new ScaleGestureDetector(context, this);
        mCanDetectZoom = canDetectZoom;
        mDelegate = delegate;
    }

    /**
     * Sets the {@link PlayerFrameGestureDetector} that corresponds to the parent view of this
     * {@link PlayerFrameGestureDetector}'s view. This is used for forwarding unconsumed touch
     * events.
     */
    void setParentGestureDetector(PlayerFrameGestureDetector parentGestureDetector) {
        mParentGestureDetector = parentGestureDetector;
    }

    /**
     * This should be called on every touch event.
     * @return Whether the event was consumed.
     */
    boolean onTouchEvent(MotionEvent event) {
        TraceEvent.begin("PlayerFrameGestureDetector.onTouchEvent");
        if (mCanDetectZoom) {
            mScaleGestureDetector.onTouchEvent(event);
        }

        if (event.getAction() == MotionEvent.ACTION_UP) {
            mDelegate.onRelease();
            // Propagate the release to the parent, this won't trigger any unexpected behavior as
            // this is only an UP event.
            if (mParentGestureDetector != null) {
                mParentGestureDetector.onTouchEvent(event);
            }
        }
        boolean ret = mGestureDetector.onTouchEvent(event);

        TraceEvent.end("PlayerFrameGestureDetector.onTouchEvent");
        return ret;
    }

    @Override
    public boolean onDown(MotionEvent e) {
        return true;
    }

    @Override
    public void onShowPress(MotionEvent e) {}

    @Override
    public boolean onSingleTapUp(MotionEvent e) {
        mDelegate.onTap((int) e.getX(), (int) e.getY());
        return true;
    }

    @Override
    public boolean onScroll(MotionEvent e1, MotionEvent e2, float distanceX, float distanceY) {
        if (mDelegate.scrollBy(distanceX, distanceY)) {
            mLastParentScrollX = 0f;
            mLastParentScrollY = 0f;
            return true;
        }

        // We need to keep track of the distance passed to the parent
        // {@link PlayerFrameGestureDetector} and accumulate them for the following events. This is
        // because if the parent view scrolls, the coordinates of the future touch events that this
        // view received will be transformed since the View associated with this
        // {@link PlayerFrameGestureDetector} moves along with the parent.
        mLastParentScrollX += distanceX;
        mLastParentScrollY += distanceY;
        if (mParentGestureDetector != null
                && mParentGestureDetector.onScroll(
                        e1, e2, mLastParentScrollX, mLastParentScrollY)) {
            return true;
        }
        mLastParentScrollX = 0f;
        mLastParentScrollY = 0f;
        return false;
    }

    @Override
    public void onLongPress(MotionEvent e) {
        mDelegate.onLongPress((int) e.getX(), (int) e.getY());
    }

    @Override
    public boolean onFling(MotionEvent e1, MotionEvent e2, float velocityX, float velocityY) {
        if (mDelegate.onFling(velocityX, velocityY)) return true;

        if (mParentGestureDetector != null) {
            return mParentGestureDetector.onFling(e1, e2, velocityX, velocityY);
        }
        return false;
    }

    @Override
    public boolean onScale(ScaleGestureDetector detector) {
        assert mCanDetectZoom;
        return mDelegate.scaleBy(
                detector.getScaleFactor(), detector.getFocusX(), detector.getFocusY());
    }

    @Override
    public boolean onScaleBegin(ScaleGestureDetector detector) {
        assert mCanDetectZoom;
        return true;
    }

    @Override
    public void onScaleEnd(ScaleGestureDetector detector) {
        assert mCanDetectZoom;
        mDelegate.scaleFinished(
                detector.getScaleFactor(), detector.getFocusX(), detector.getFocusY());
    }
}
