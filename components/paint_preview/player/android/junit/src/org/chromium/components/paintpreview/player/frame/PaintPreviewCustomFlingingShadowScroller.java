// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

import android.content.Context;
import android.widget.OverScroller;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

/** A custom shadow of {@link Scroller} that supports fake flinging. */
@Implements(OverScroller.class)
public class PaintPreviewCustomFlingingShadowScroller {
    private int mFinalX;
    private int mFinalY;

    private int mCurrX;
    private int mCurrY;

    private boolean mFinished;

    public PaintPreviewCustomFlingingShadowScroller() {}

    @Implementation
    public void __constructor__(Context context) {
        mFinalX = 0;
        mFinalY = 0;
        mCurrX = 0;
        mCurrY = 0;
        mFinished = true;
    }

    @Implementation
    public int getFinalX() {
        return mFinalX;
    }

    @Implementation
    public int getFinalY() {
        return mFinalY;
    }

    @Implementation
    public int getCurrX() {
        return mCurrX;
    }

    @Implementation
    public int getCurrY() {
        return mCurrY;
    }

    @Implementation
    public boolean isFinished() {
        return mFinished;
    }

    @Implementation
    public void fling(
            int startX,
            int startY,
            int velocityX,
            int velocityY,
            int minX,
            int maxX,
            int minY,
            int maxY) {
        mFinished = false;
        mCurrX = startX;
        mCurrY = startY;
        mFinalX = startX;
        mFinalY = startY;
        // If there is any velocity then head maximally in that direction.
        if (velocityX != 0) {
            mFinalX = (velocityX > 0) ? minX : maxX;
        }
        if (velocityY != 0) {
            mFinalY = (velocityY > 0) ? minY : maxY;
        }
    }

    @Implementation
    public boolean computeScrollOffset() {
        mFinished = true;
        mCurrX = mFinalX;
        mCurrY = mFinalY;
        return false;
    }
}
