// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.Shader;
import android.graphics.SweepGradient;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;

import androidx.annotation.ColorInt;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * A drawable that combines a {@link GradientDrawable} background with an optional rainbow hairline
 * stroke.
 */
@NullMarked
public class LocationBarBackgroundDrawable extends Drawable {
    private final GradientDrawable mBackgroundGradient;
    private final Paint mPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Path mPath = new Path();
    private final Rect mInsets = new Rect();
    private final Rect mEffectiveBounds = new Rect();
    private final int[] mColors;

    private final float mStrokePx;
    private float mCornerRadiusPx;
    private @Nullable Shader mShader;
    private boolean mDrawHairline;

    /**
     * Creates a new instance of {@link LocationBarBackgroundDrawable}.
     *
     * @param context The context to use.
     * @param cornerRadiusPx The corner radius in pixels.
     * @param strokePx The stroke width in pixels.
     */
    public LocationBarBackgroundDrawable(Context context, float cornerRadiusPx, float strokePx) {
        this(
                (GradientDrawable)
                        assumeNonNull(
                                context.getDrawable(R.drawable.modern_toolbar_text_box_background)),
                cornerRadiusPx,
                strokePx,
                new int[] {
                    0xFFE91E63, // pink
                    0xFFFF5722, // deep orange
                    0xFFFFC107, // amber
                    0xFF8BC34A, // light green
                    0xFF03A9F4, // light blue
                    0xFF3F51B5, // indigo
                    0xFF9C27B0, // purple
                    0xFFE91E63 // back to pink to close the loop
                });
    }

    /**
     * Creates a new instance of {@link LocationBarBackgroundDrawable}.
     *
     * @param backgroundGradient The background gradient to use.
     * @param cornerRadiusPx The corner radius in pixels.
     * @param strokePx The stroke width in pixels.
     * @param colors The colors to use for the rainbow hairline.
     */
    public LocationBarBackgroundDrawable(
            GradientDrawable backgroundGradient,
            float cornerRadiusPx,
            float strokePx,
            int[] colors) {
        mBackgroundGradient = backgroundGradient;
        mCornerRadiusPx = cornerRadiusPx;
        mStrokePx = strokePx;
        mColors = colors;
        mPaint.setStyle(Paint.Style.STROKE);
        mPaint.setStrokeWidth(mStrokePx);

        mBackgroundGradient.mutate();
    }

    /** Computes the effective drawing bounds by applying the insets to the drawable's bounds. */
    private void computeEffectiveBounds() {
        Rect bounds = getBounds();
        mEffectiveBounds.set(
                bounds.left + mInsets.left,
                bounds.top + mInsets.top,
                bounds.right - mInsets.right,
                bounds.bottom - mInsets.bottom);
    }

    @Override
    public void setBounds(int left, int top, int right, int bottom) {
        super.setBounds(left, top, right, bottom);
        computeEffectiveBounds();
        mBackgroundGradient.setBounds(mEffectiveBounds);
    }

    @Override
    protected void onBoundsChange(Rect bounds) {
        super.onBoundsChange(bounds);
        computeEffectiveBounds();

        // Rebuild path.
        mPath.reset();
        mPath.addRoundRect(
                mEffectiveBounds.left,
                mEffectiveBounds.top,
                mEffectiveBounds.right,
                mEffectiveBounds.bottom,
                mCornerRadiusPx,
                mCornerRadiusPx,
                Path.Direction.CW);

        // Rebuild shader centered on this view.
        mShader =
                new SweepGradient(
                        mEffectiveBounds.centerX(),
                        mEffectiveBounds.centerY(),
                        mColors,
                        /* positions= */ null);
        mPaint.setShader(mShader);
    }

    @Override
    public void draw(Canvas canvas) {
        mBackgroundGradient.draw(canvas);
        if (mDrawHairline) {
            canvas.drawPath(mPath, mPaint);
        }
    }

    /** Controls the opacity of the rainbow outline. */
    @Override
    public void setAlpha(int alpha) {
        mPaint.setAlpha(alpha);
        invalidateSelf();
    }

    /** Returns the opacity of the rainbow outline. */
    @Override
    public int getOpacity() {
        return PixelFormat.TRANSLUCENT;
    }

    @Override
    public void setColorFilter(@Nullable ColorFilter colorFilter) {
        mPaint.setColorFilter(colorFilter);
        invalidateSelf();
    }

    /**
     * Sets the insets for the drawable.
     *
     * @param left The left inset in pixels.
     * @param top The top inset in pixels.
     * @param right The right inset in pixels.
     * @param bottom The bottom inset in pixels.
     */
    public void setInsets(@Px int left, @Px int top, @Px int right, @Px int bottom) {
        mInsets.set(left, top, right, bottom);
        computeEffectiveBounds();
        mBackgroundGradient.setBounds(mEffectiveBounds);
        onBoundsChange(getBounds());
        invalidateSelf();
    }

    /**
     * Sets the background color of the drawable.
     *
     * @param color The color to set as the background.
     */
    public void setBackgroundColor(@ColorInt int color) {
        mBackgroundGradient.setColor(color);
        invalidateSelf();
    }

    /**
     * Sets whether the rainbow hairline should be drawn.
     *
     * @param shouldDrawHairline True to draw the hairline, false otherwise.
     */
    public void setDrawHairline(boolean shouldDrawHairline) {
        if (mDrawHairline == shouldDrawHairline) return;
        mDrawHairline = shouldDrawHairline;
        invalidateSelf();
    }

    /**
     * Sets the corner radius for the drawable.
     *
     * @param radius The corner radius in pixels.
     */
    public void setCornerRadius(@Px int radius) {
        mBackgroundGradient.setCornerRadius(radius);
        mCornerRadiusPx = radius;
        onBoundsChange(getBounds());
        invalidateSelf();
    }

    @VisibleForTesting
    Rect getEffectiveBoundsForTesting() {
        return mEffectiveBounds;
    }

    @VisibleForTesting
    float getCornerRadiusForTesting() {
        return mCornerRadiusPx;
    }

    @VisibleForTesting
    boolean getDrawHairlineForTesting() {
        return mDrawHairline;
    }

    @VisibleForTesting
    Path getPathForTesting() {
        return mPath;
    }

    @VisibleForTesting
    Paint getPaintForTesting() {
        return mPaint;
    }
}
