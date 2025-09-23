// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.web_app_header;

import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;

import org.chromium.build.annotations.NullMarked;

/**
 * Custom drawable that renders two rectangular bars on the left/right sides of the view. All
 * remaining space in the middle should be transparent.
 *
 * <p>If either bar width is -1, then this will render one continuous rectangular bar for the entire
 * drawable bounds.
 */
@NullMarked
class CutoutDrawable extends ColorDrawable {

    private final Paint mPaint;
    private float mLeftBarWidth;
    private float mRightBarWidth;

    CutoutDrawable() {
        mPaint = new Paint();
        mPaint.setStyle(Paint.Style.FILL);
    }

    public void setBarWidths(float leftBarWidth, float rightBarWidth) {
        mLeftBarWidth = leftBarWidth;
        mRightBarWidth = rightBarWidth;
        invalidateSelf();
    }

    @Override
    public void setColor(int color) {
        mPaint.setColor(color);
        super.setColor(color);
    }

    @Override
    public void draw(Canvas canvas) {
        Rect bounds = getBounds();

        // -1 for either side indicates that there should not be any cutout in the middle
        if (mLeftBarWidth < 0 || mRightBarWidth < 0) {
            canvas.drawRect(bounds.left, bounds.top, bounds.right, bounds.bottom, mPaint);
            return;
        }

        float leftBarRightX = bounds.left + mLeftBarWidth;
        canvas.drawRect(bounds.left, bounds.top, leftBarRightX, bounds.bottom, mPaint);

        float rightBarLeftX = bounds.right - mRightBarWidth;
        canvas.drawRect(rightBarLeftX, bounds.top, bounds.right, bounds.bottom, mPaint);
    }
}
