// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.messages.snackbar;

import android.content.Context;
import android.util.Pair;
import android.view.MotionEvent;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.ui.messages.R;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.ScrollDirection;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.SwipeHandler;

/** A handler to respond touch events of swipe. */
@NullMarked
public class SnackbarSwipeHandler implements SwipeHandler {

    /** A delegate to dismiss or update view's position. */
    public interface Delegate {
        void dismiss();

        void resetPosition();

        /**
         * Update position based on scrolled distance.
         *
         * @param dx The distance along the X axis that has been scrolled.
         * @param dy The distance along the Y axis that has been scrolled
         */
        void updatePosition(float dx, float dy);
    }

    private float mTranslateX;
    private final Delegate mDelegate;
    private final Context mContext;

    /**
     * @param context A {@link Context} of the snackbar.
     * @param delegate A delegate to dismiss or update view's position.
     */
    public SnackbarSwipeHandler(Context context, Delegate delegate) {
        mContext = context;
        mDelegate = delegate;
    }

    @Override
    public void onSwipeUpdated(
            MotionEvent current, float tx, float ty, float distanceX, float distanceY) {
        mTranslateX = tx;
        mDelegate.updatePosition(tx, ty);
    }

    @Override
    public void onSwipeFinished() {
        if (Math.abs(mTranslateX)
                >= mContext.getResources()
                        .getDimensionPixelSize(R.dimen.snackbar_horizontal_hide_threshold)) {
            mDelegate.dismiss();
        } else {
            mDelegate.resetPosition();
        }
        mTranslateX = 0f;
    }

    @Override
    public boolean isSwipeEnabled(int direction) {
        return direction == ScrollDirection.LEFT || direction == ScrollDirection.RIGHT;
    }

    public Pair<Float, Float> getTranslateData() {
        return Pair.create(mTranslateX, 0f);
    }
}
