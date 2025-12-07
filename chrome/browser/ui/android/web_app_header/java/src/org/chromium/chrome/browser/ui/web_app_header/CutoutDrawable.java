// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.web_app_header;

import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffXfermode;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.List;

/**
 * Custom drawable that fills in the entire area with a solid color, but will leave certain "cutout"
 * regions transparent. These regions can be specified as `Rect`s via the `setCutouts` method.
 */
@NullMarked
class CutoutDrawable extends ColorDrawable {

    private final Paint mBackgroundPaint;
    private final Paint mCutoutPaint;
    private @Nullable List<Rect> mCutouts;

    CutoutDrawable() {
        mBackgroundPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mBackgroundPaint.setStyle(Paint.Style.FILL);

        mCutoutPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mCutoutPaint.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.CLEAR));
    }

    public void setCutouts(@Nullable List<Rect> cutouts) {
        mCutouts = cutouts;
        invalidateSelf();
    }

    @Override
    public void setColor(int color) {
        mBackgroundPaint.setColor(color);
        super.setColor(color);
    }

    @Override
    public void draw(Canvas canvas) {
        Rect bounds = getBounds();

        int saveCount =
                canvas.saveLayer(bounds.left, bounds.top, bounds.right, bounds.bottom, null);

        canvas.drawRect(bounds.left, bounds.top, bounds.right, bounds.bottom, mBackgroundPaint);

        if (mCutouts != null && !mCutouts.isEmpty()) {
            for (Rect rect : mCutouts) {
                canvas.drawRect(rect, mCutoutPaint);
            }
        }

        canvas.restoreToCount(saveCount);
    }
}
