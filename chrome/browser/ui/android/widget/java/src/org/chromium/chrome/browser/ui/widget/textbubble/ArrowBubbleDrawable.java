// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.widget.textbubble;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.ShapeDrawable;
import android.graphics.drawable.shapes.RoundRectShape;
import android.support.v4.graphics.drawable.DrawableCompat;

import androidx.annotation.ColorInt;

import org.chromium.chrome.browser.ui.widget.R;

/**
 * A {@link Drawable} that is a bubble with an arrow pointing out of either the top or bottom.
 */
class ArrowBubbleDrawable extends Drawable implements Drawable.Callback {
    private final Rect mCachedBubblePadding = new Rect();

    private final int mRadiusPx;
    private final int mArrowWidthPx;
    private final int mArrowHeightPx;

    private final Path mArrowPath;
    private final Paint mArrowPaint;

    private final Drawable mBubbleDrawable;

    private int mArrowXOffsetPx;
    private boolean mArrowOnTop;
    private boolean mShowArrow;

    public ArrowBubbleDrawable(Context context) {
        mRadiusPx = context.getResources().getDimensionPixelSize(R.dimen.text_bubble_corner_radius);
        mArrowWidthPx =
                context.getResources().getDimensionPixelSize(R.dimen.text_bubble_arrow_width);
        mArrowHeightPx =
                context.getResources().getDimensionPixelSize(R.dimen.text_bubble_arrow_height);

        // Set up the arrow path and paint for drawing.
        mArrowPath = new Path();
        mArrowPath.setFillType(Path.FillType.EVEN_ODD);
        mArrowPath.moveTo(-mArrowWidthPx / 2.f, mArrowHeightPx);
        mArrowPath.lineTo(0, 0);
        mArrowPath.lineTo(mArrowWidthPx / 2.f, mArrowHeightPx);
        mArrowPath.lineTo(-mArrowWidthPx / 2.f, mArrowHeightPx);
        mArrowPath.close();

        mArrowPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mArrowPaint.setColor(Color.WHITE);
        mArrowPaint.setStyle(Paint.Style.FILL);

        mBubbleDrawable = DrawableCompat.wrap(new ShapeDrawable(
                new RoundRectShape(new float[] {mRadiusPx, mRadiusPx, mRadiusPx, mRadiusPx,
                                           mRadiusPx, mRadiusPx, mRadiusPx, mRadiusPx},
                        null, null)));

        mBubbleDrawable.setCallback(this);
    }

    /**
     * Updates the arrow offset and whether or not it is on top.
     * @param arrowXOffsetPx The horizontal offset of where the arrow should be in pixels.  This
     *                       offset is where the center of the arrow will position itself.
     * @param arrowOnTop     Whether or not the arrow should be on top of the bubble.
     */
    public void setPositionProperties(int arrowXOffsetPx, boolean arrowOnTop) {
        if (arrowXOffsetPx == mArrowXOffsetPx && arrowOnTop == mArrowOnTop) return;
        mArrowXOffsetPx = arrowXOffsetPx;
        mArrowOnTop = arrowOnTop;
        onBoundsChange(getBounds());
        invalidateSelf();
    }

    /**
     * @return The spacing needed on the left side of the {@link Drawable} for the arrow to fit.
     */
    public int getArrowLeftSpacing() {
        mBubbleDrawable.getPadding(mCachedBubblePadding);
        return mRadiusPx + mCachedBubblePadding.left + mArrowWidthPx / 2;
    }

    /**
     * @return The spacing needed on the right side of the {@link Drawable} for the arrow to fit.
     */
    public int getArrowRightSpacing() {
        mBubbleDrawable.getPadding(mCachedBubblePadding);
        return mRadiusPx + mCachedBubblePadding.right + mArrowWidthPx / 2;
    }

    /**
     * @return Whether or not the arrow is currently drawing on top of this {@link Drawable}.
     */
    public boolean isArrowOnTop() {
        return mArrowOnTop;
    }

    /**
     * @return Whether or not an arrow is currently shown.
     */
    public boolean isShowingArrow() {
        return mShowArrow;
    }

    /**
     * @param showArrow Whether the bubble should have an arrow.
     */
    public void setShowArrow(boolean showArrow) {
        mShowArrow = showArrow;
        invalidateSelf();
    }

    /**
     * @param color The color to make the bubble and arrow.
     */
    public void setBubbleColor(@ColorInt int color) {
        DrawableCompat.setTint(mBubbleDrawable, color);
        mArrowPaint.setColor(color);
        invalidateSelf();
    }

    // Drawable.Callback implementation.
    @Override
    public void invalidateDrawable(Drawable who) {
        invalidateSelf();
    }

    @Override
    public void scheduleDrawable(Drawable who, Runnable what, long when) {
        scheduleSelf(what, when);
    }

    @Override
    public void unscheduleDrawable(Drawable who, Runnable what) {
        unscheduleSelf(what);
    }

    // Drawable implementation.
    @Override
    public void draw(Canvas canvas) {
        mBubbleDrawable.draw(canvas);

        if (mShowArrow) {
            canvas.save();

            // If the arrow is on the bottom, flip the arrow before drawing.
            if (!mArrowOnTop) {
                int arrowCenterYPx = getBounds().height() - mArrowHeightPx / 2;
                canvas.scale(1, -1, mArrowXOffsetPx, arrowCenterYPx);
                canvas.translate(0, arrowCenterYPx - mArrowHeightPx / 2);
            }
            canvas.translate(mArrowXOffsetPx, 0);
            canvas.drawPath(mArrowPath, mArrowPaint);
            canvas.restore();
        }
    }

    @Override
    protected void onBoundsChange(Rect bounds) {
        super.onBoundsChange(bounds);
        if (bounds == null) return;

        // Calculate the bubble bounds.  Account for the arrow size requiring more space.
        mBubbleDrawable.getPadding(mCachedBubblePadding);
        mBubbleDrawable.setBounds(bounds.left,
                bounds.top + (mArrowOnTop ? (mArrowHeightPx - mCachedBubblePadding.top) : 0),
                bounds.right,
                bounds.bottom - (mArrowOnTop ? 0 : (mArrowHeightPx - mCachedBubblePadding.bottom)));
    }

    @Override
    public void setAlpha(int alpha) {
        mBubbleDrawable.setAlpha(alpha);
        mArrowPaint.setAlpha(alpha);
        invalidateSelf();
    }

    @Override
    public void setColorFilter(ColorFilter cf) {
        assert false : "Unsupported";
    }

    @Override
    public int getOpacity() {
        return PixelFormat.TRANSLUCENT;
    }

    @Override
    public boolean getPadding(Rect padding) {
        mBubbleDrawable.getPadding(padding);

        padding.set(padding.left, Math.max(padding.top, mArrowOnTop ? mArrowHeightPx : 0),
                padding.right, Math.max(padding.bottom, mArrowOnTop ? 0 : mArrowHeightPx));
        return true;
    }
}